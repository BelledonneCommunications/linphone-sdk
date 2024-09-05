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
#include <soci/soci.h>

#include "bctoolbox/exception.hh"

#include <algorithm> //copy_n


using namespace::std;
using namespace::soci;
using namespace::lime;

namespace lime {
	namespace {// anonymous namespace for local functions/classes
	/**
	 * @brief the possible status of session regarding the Local Storage
	 *
	 * used to pick a subset of session to be saved in DB
	*/
	enum class DRSessionDbStatus : uint8_t {
		clean, /**< session in cache match the one in local storage */
		dirty_encrypt, /**< an encrypt was performed modifying part of the cached session */
		dirty_decrypt, /**< a dencrypt was performed modifying part of the cached session */
		dirty_ratchet_receiving, /**< a EC onlyratchet step was performed modifying part of cached session, using a new peer pk */
		dirty_kem_ratchet_receiving, /**< a KEM/EC ratchet step was performed modifying part of cached session, using a new peer pk */
		dirty_ratchet_sending, /**< a ratchet step was performed modifying part of cached session, using a new self pk */
		dirty /**< the whole session data must be saved to local storage */
	};


	/** Double Ratchet Message keys : 32 bytes of encryption key followed by 16 bytes of IV */
	using DRMKey = lime::sBuffer<lime::settings::DRMessageKeySize+lime::settings::DRMessageIVSize>;

	/**
	 * @brief Chain storing the DH and MKs associated with Nr(uint16_t map index)
	 * @tparam Curve	The elliptic curve to use: C255 or C448
	 */
	template <typename Curve>
	struct ReceiverKeyChain {
		std::vector<uint8_t> DHrIndex; /**< peer public key(or a hash of it) identifying this chain */
		std::unordered_map<uint16_t, DRMKey> messageKeys; /**< message keys indexed by Nr */
		/**
		 * Start a new empty chain
		 * @param[in]	key	the peer DH public key used on this chain
		 */
		ReceiverKeyChain(const std::vector<uint8_t> &keyIndex) :DHrIndex{keyIndex}, messageKeys{} {};
	};

	/****************************************************************************/
	/* Helpers functions not part of DRi class                                  */
	/****************************************************************************/
	/* Key derivation functions : KDF_RK (root key derivation function, for Asymmetric ratchet) and KDF_CK(chain key derivation function, for symmetric ratchet) */
	/**
	 * @brief Key Derivation Function used in Root key/Diffie-Hellman Ratchet chain.
	 *
	 *      Use HKDF (see RFC5869) to derive CK and RK in one derivation
	 *
	 * @param[in,out]	RK	Input buffer used as salt also to store the 32 first byte of output key material
	 * @param[out]		CK	Output buffer, last 32 bytes of output key material
	 * @param[in]		dhOut	Buffer used as input key material
	 */
	template <typename Curve>
	static void KDF_RK(DRChainKey &RK, DRChainKey &CK, const X<Curve, lime::Xtype::sharedSecret> &dhOut) noexcept {
		// Ask for twice the size of a DRChainKey for HKDF output
		lime::sBuffer<2*lime::settings::DRChainKeySize> HKDFoutput{};
		HMAC_KDF<SHA512>(RK.data(), RK.size(), dhOut.data(), dhOut.size(), lime::settings::hkdf_DRChainKey_info.data(), lime::settings::hkdf_DRChainKey_info.size(), HKDFoutput.data(), HKDFoutput.size());

		// First half of the output goes to RootKey (RK)
		std::copy_n(HKDFoutput.cbegin(), lime::settings::DRChainKeySize, RK.begin());
		// Second half of the output goes to ChainKey (CK)
		std::copy_n(HKDFoutput.cbegin()+lime::settings::DRChainKeySize, lime::settings::DRChainKeySize, CK.begin());
	}

	/**
	 * @brief Key Derivation Function used in Root key/Diffie-Hellman/KEM Ratchet chain.
	 *
	 *      Use HKDF (see RFC5869) to derive CK and RK in one derivation
	 *  The key derivation uses as input material the DH and KEM shared secrets but also a transcript of all public material used to compute them
	 *
	 * @param[in,out]	RK	Input buffer used as salt also to store the 32 first byte of output key material
	 * @param[out]		CK	Output buffer, last 32 bytes of output key material
	 * @param[in]		dhOut	Buffer used as input key material
	 * @param[in]		kemOut	Buffer used as input key material
	 * @param[in]		ECPkSender 		EC Public Key Sender
	 * @param[in]		ECPkReceiver	EC Public Key Receiver
	 * @param[in]		KEMPk 			KEM public key
	 * @param[in] 		KEMCt  			KEM cipher text
	 */
	template <typename Curve>
	static void KEM_KDF_RK(DRChainKey &RK, DRChainKey &CK,
						const X<typename Curve::EC, lime::Xtype::sharedSecret> &dhOut, const K<typename Curve::KEM, lime::Ktype::sharedSecret> &kemOut,
						const X<typename Curve::EC, lime::Xtype::publicKey> &ECPkSender, const X<typename Curve::EC, lime::Xtype::publicKey> &ECPkReceiver,
						const K<typename Curve::KEM, lime::Ktype::publicKey> &KEMPk, const K<typename Curve::KEM, lime::Ktype::cipherText> &KEMCt) noexcept {
		// concatenate dhOut, kemOut and the transcript
		sBuffer<
		X<Curve, lime::Xtype::sharedSecret>::ssize() + K<Curve, lime::Ktype::sharedSecret>::ssize()
		+ X<Curve, lime::Xtype::publicKey>::ssize()*2
		+ K<Curve, lime::Ktype::publicKey>::ssize() + K<Curve, lime::Ktype::cipherText>::ssize()
		>ikm{};
		std::copy_n(dhOut.cbegin(), X<Curve, lime::Xtype::sharedSecret>::ssize(), ikm.begin());
		auto inputIndex = X<Curve, lime::Xtype::sharedSecret>::ssize();
		std::copy_n(kemOut.cbegin(), K<Curve, lime::Ktype::sharedSecret>::ssize(), ikm.begin() + inputIndex);
		inputIndex += K<Curve, lime::Ktype::sharedSecret>::ssize();
		std::copy_n(ECPkSender.cbegin(), X<Curve, lime::Xtype::publicKey>::ssize(), ikm.begin() + inputIndex);
		inputIndex += X<Curve, lime::Xtype::publicKey>::ssize();
		std::copy_n(ECPkReceiver.cbegin(), X<Curve, lime::Xtype::publicKey>::ssize(), ikm.begin() + inputIndex);
		inputIndex += X<Curve, lime::Xtype::publicKey>::ssize();
		std::copy_n(KEMPk.cbegin(), K<Curve, lime::Ktype::publicKey>::ssize(), ikm.begin() + inputIndex);
		inputIndex += K<Curve, lime::Ktype::publicKey>::ssize();
		std::copy_n(KEMCt.cbegin(), K<Curve, lime::Ktype::cipherText>::ssize(), ikm.begin() + inputIndex);
		// Ask for twice the size of a DRChainKey for HKDF output
		lime::sBuffer<2*lime::settings::DRChainKeySize> HKDFoutput{};
		HMAC_KDF<SHA512>(RK.data(), RK.size(), ikm.data(), ikm.size(), lime::settings::hkdf_DRChainKey_info.data(), lime::settings::hkdf_DRChainKey_info.size(), HKDFoutput.data(), HKDFoutput.size());

		// First half of the output goes to RootKey (RK)
		std::copy_n(HKDFoutput.cbegin(), lime::settings::DRChainKeySize, RK.begin());
		// Second half of the output goes to ChainKey (CK)
		std::copy_n(HKDFoutput.cbegin()+lime::settings::DRChainKeySize, lime::settings::DRChainKeySize, CK.begin());
	}

	/** constant used as input of HKDF like function, see double ratchet spec section 5.2 - KDF_CK */
	const std::array<uint8_t,1> hkdf_ck_info{{0x02}};
	/** constant used as input of HKDF like function, see double ratchet spec section 5.2 - KDF_CK */
	const std::array<uint8_t,1> hkdf_mk_info{{0x01}};

	/**
	 * @brief Key Derivation Function used in Symmetric key ratchet chain.
	 *
	 *      Implemented according to Double Ratchet spec section 5.2 using HMAC-SHA512
	 *      @code{.unparsed}
	 *		MK = HMAC-SHA512(CK, hkdf_mk_info) // get 48 bytes of it: first 32 to be key and last 16 to be IV
	 *		CK = HMAC-SHA512(CK, hkdf_ck_info)
	 *              hkdf_ck_info and hkdf_mk_info being a distincts constants (0x02 and 0x01 as suggested in double ratchet - section 5.2)
	 *      @endcode
	 *
	 *		The EC/KEM version includes a modification from https://eprint.iacr.org/2024/220.pdf section 4.2: use the chain index in the derivation
	 * 		append the derivation index to hkdf_mk_info and hkdf_ck_info
	 *
	 * @param[in,out]	CK	Input/output buffer used as key to compute MK and then next CK
	 * @param[out]		MK	Message Key(32 bytes) and IV(16 bytes) computed from HMAC_SHA512 keyed with CK
	 */
	template <typename Curve>
	static void KDF_CK(DRChainKey &CK, DRMKey &MK, uint16_t chainIndex, typename std::enable_if_t<!std::is_base_of_v<genericKEM, Curve>, bool> = true) noexcept {
		// derive MK and IV from CK and constant
		HMAC<SHA512>(CK.data(), CK.size(), hkdf_mk_info.data(), hkdf_mk_info.size(), MK.data(), MK.size());

		// use temporary buffer, not likely that output and key could be the same buffer
		DRChainKey tmp;
		HMAC<SHA512>(CK.data(), CK.size(), hkdf_ck_info.data(), hkdf_ck_info.size(), tmp.data(), tmp.size());
		CK = tmp;
	}
	template <typename Curve>
	static void KDF_CK(DRChainKey &CK, DRMKey &MK, uint16_t chainIndex, typename std::enable_if_t<std::is_base_of_v<genericKEM, Curve>, bool> = true) noexcept {
		// derive MK and IV from CK and constant
		std::array<uint8_t,3> label{hkdf_mk_info[0], static_cast<uint8_t>(chainIndex>>8), static_cast<uint8_t>(0xFF&chainIndex)};
		HMAC<SHA512>(CK.data(), CK.size(), label.data(), label.size(), MK.data(), MK.size());

		// use temporary buffer, not likely that output and key could be the same buffer
		DRChainKey tmp;
		label[0]=hkdf_ck_info[0];
		HMAC<SHA512>(CK.data(), CK.size(), label.data(), label.size(), tmp.data(), tmp.size());
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
	 * @brief helper class and functions to parse Double Ratchet message header and access its components
	 *
	 */
	template <typename Curve, bool = std::is_base_of_v<genericKEM, Curve>>
	class DRHeader;

	// EC only version
	template <typename Curve>
	class DRHeader<Curve, false> {
		private:
			uint16_t m_Ns,m_PN; /**<  Sender chain and Previous Sender chain indexes. */
			lime::X<Curve, lime::Xtype::publicKey> m_DHr; /**< Remote - from the receiver point of view - public key */
			bool m_valid; /**< is this header valid? */
			size_t m_size; /**< store the size of parsed header */
			bool m_payload_direct_encryption; /**< flag to store the message encryption mode: in the double ratchet packet or using a random key to encrypt it separately and encrypt the key in the DR packet */

		public:
			/// read-only accessor to Sender Chain index (Ns)
			uint16_t Ns(void) const {return m_Ns;}
			/// read-only accessor to Previous Sender Chain index (PN)
			uint16_t PN(void) const {return m_PN;}
			/// read-only accessor to peer Double Ratchet public key
			const lime::X<Curve, lime::Xtype::publicKey> &DHr(void) const {return m_DHr;}
			/// is this header valid? (property is set by constructor/parser)
			bool valid(void) const {return m_valid;}
			/// what encryption mode is advertised in this header
			bool payloadDirectEncryption(void) const {return m_payload_direct_encryption;}
			/// is there a KEM public key in this header? Never for EC only.
			bool havePKIndex(void) const {return false;}
			/// read-only accessor to the size of parsed header
			size_t size(void) {return m_size;}
			/// compare the DHr in the header to the one given in parameter. Return true if they match
			bool isDHr(const ARrKey<Curve> &DHr) {
				return (DHr.publicKey() == m_DHr);
			}
			/// compare the given DHs with the KEM DHsIndex present in header
			bool isDHsIndex(const ARsKey<Curve> &DHs) {
				return false; // EC only version never have any KEM DHs Index in header
			}
			std::vector<uint8_t> getDHrIndex(void) {
				std::vector<uint8_t>index(lime::settings::DRPkIndexSize);
				HMAC<SHA512>(nullptr, 0, m_DHr.data(), m_DHr.size(), index.data(), lime::settings::DRPkIndexSize);
				return index;
			}
			// mostly to make compiler happy as we have no KEM ratchet when using EC only base
			std::vector<uint8_t> getDHsIndex(void) {
				return std::vector<uint8_t>{};
			}
			/* ctor/dtor */
			DRHeader() = delete;
			DRHeader(const std::vector<uint8_t> header) : m_Ns{0}, m_PN{0}, m_DHr{}, m_valid{false}, m_size{0}{ // init valid to false and check during parsing if all is ok
				// make sure we have at least enough data to parse version<1 byte> || message type<1 byte> || curve Id<1 byte> || [x3dh init] || OPk flag without any ulterior checks on size
				if (header.size()<3 || header.size()<lime::double_ratchet_protocol::headerSize<Curve>(header[1])) {
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
						m_size = lime::double_ratchet_protocol::headerSize<Curve>(header[1]); // headerSize is the size when no X3DH init is present
						size_t index = 3;
						if (messageType & static_cast<uint8_t>(lime::double_ratchet_protocol::DR_message_type::X3DH_init_flag)) {

							size_t x3dh_initMessageSize = lime::double_ratchet_protocol::X3DHinitSize<Curve>(header[3] == 1);
							m_size += x3dh_initMessageSize;
							index += x3dh_initMessageSize;
						}
						if (header.size() >=  m_size) { //header shall be actually longer because buffer pass is the whole message
							m_Ns = header[index]<<8|header[index+1];
							index += 2;
							m_PN = header[index]<<8|header[index+1];
							index += 2;
							m_DHr.assign(header.cbegin()+index);
							m_valid = true;
						}
					}
					break;

					default: // just do nothing, we do not know this version of header, don't parse anything and leave its valid flag to false
					break;
				}
			 }

			~DRHeader() {};

			void dump(std::ostringstream &os, std::string indent="        ") const {
				if (m_valid) {}
					os<<std::endl<<indent<<"Ns: 0x"<<std::dec<<std::setw(4) << std::setfill('0') << m_Ns;
					os<<std::endl<<indent<<"PN: 0x"<<std::dec<<std::setw(4) << std::setfill('0') << m_PN;
					os<<std::endl<<indent<<"payload direct encryption: "<<m_payload_direct_encryption;
					os<<std::endl<<indent<<"Pk: ";
					hexStr(os, m_DHr.data(),  m_DHr.size());
			}
	};

	// EC+KEM version
	template <typename Algo>
	class DRHeader<Algo, true> {
		private:
			uint16_t m_Ns,m_PN; /**<  Sender chain and Previous Sender chain indexes. */
			// TODO: union
			lime::X<typename Algo::EC, lime::Xtype::publicKey> m_EC_DHr; /**< Remote - from the receiver point of view - public key */
			std::vector<uint8_t> m_KEMDHrIndex; /**< KEM Public key index of remote key - as stored in DB to index skipped message chain */
			std::vector<uint8_t> m_KEMDHsIndex; /**< KEM Public key index of self key - as seen by peer: allow to confirm peer got our current key */
			// TODO OR
			typename lime::ARrKey<Algo>::serializedBuffer m_DHr; /**< Remote - from the receiver point of view - public key */

			bool m_valid; /**< is this header valid? */
			size_t m_size; /**< store the size of parsed header */
			bool m_payload_direct_encryption; /**< flag to store the message encryption mode: in the double ratchet packet or using a random key to encrypt it separately and encrypt the key in the DR packet */
			bool m_havePkIndex; /**< The header holds KEM Pk indexes and not the actual PK/CT*/

		public:
			/// read-only accessor to Sender Chain index (Ns)
			uint16_t Ns(void) const {return m_Ns;}
			/// read-only accessor to Previous Sender Chain index (PN)
			uint16_t PN(void) const {return m_PN;}
			/// read-only accessor to peer EC Double Ratchet public key
			const lime::X<typename Algo::EC, lime::Xtype::publicKey> &ECDHr(void) const {return m_EC_DHr;}
			/// read-only accessor to peer whole set of keys/ct :EC pk, KEM pk, KEM ct
			const typename lime::ARsKey<Algo>::serializedPublicBuffer &DHr(void) const {return m_DHr;}
			/// is this header valid? (property is set by constructor/parser)
			bool valid(void) const {return m_valid;}
			/// what encryption mode is advertised in this header
			bool payloadDirectEncryption(void) const {return m_payload_direct_encryption;}
			/// is there a KEM public key in this header or just an index?
			bool havePKIndex(void) const {return m_havePkIndex;}
			/// read-only accessor to the size of parsed header
			size_t size(void) {return m_size;}
			/// compare the DHr in the header to the one given in parameter. Return true if they match
			bool isDHr(const ARrKey<Algo> &DHr) {
				if (m_havePkIndex) {
					return ((DHr.ECPublicKey() == m_EC_DHr)
						&& (DHr.getKEMIndex() == m_KEMDHrIndex));
				} else { // we have a whole DHr
					return (DHr.serialize() == m_DHr);
				}

			}
			/// compare the given DHs with the KEM DHsIndex present in header
			bool isDHsIndex(const ARsKey<Algo> &DHs) {
				return (m_havePkIndex && (DHs.getKEMIndex() == m_KEMDHsIndex));
				return false; // EC only version never have any KEM DHs Index in header
			}
			/// Return the index used in Db:
			/// - first part is the ECDH index
			/// - second part is KEM index
			std::vector<uint8_t> getDHrIndex(void) {
				if (m_havePkIndex) { // we have directly the KEM index in header
					std::vector<uint8_t>index(lime::settings::DRPkIndexSize);
					HMAC<SHA512>(nullptr, 0, m_EC_DHr.data(), m_EC_DHr.size(), index.data(), lime::settings::DRPkIndexSize);
					index.insert(index.end(), m_KEMDHrIndex.cbegin(), m_KEMDHrIndex.cend());
					return index;
				} else {// compute the two parts from the DHr buffer holding: EC pk || KEM pk || KEM ct
					std::vector<uint8_t>index(2*lime::settings::DRPkIndexSize);
					HMAC<SHA512>(nullptr, 0, m_DHr.data(), lime::X<Algo, lime::Xtype::publicKey>::ssize(), index.data(), lime::settings::DRPkIndexSize);
					HMAC<SHA512>(nullptr, 0,
								 m_DHr.data() + lime::X<Algo, lime::Xtype::publicKey>::ssize(), K<Algo, lime::Ktype::publicKey>::ssize()+K<Algo, lime::Ktype::cipherText>::ssize(),
								 index.data() + lime::settings::DRPkIndexSize, lime::settings::DRPkIndexSize);
					return index;
				}
			}
			// return DHs index if any, empty vector otherwise
			std::vector<uint8_t> getDHsIndex(void) {
				if (m_havePkIndex) {
					return m_KEMDHsIndex;
				} else {
					return std::vector<uint8_t>{};
				}
			}
			/* ctor/dtor */
			DRHeader() = delete;
			DRHeader(const std::vector<uint8_t> header) : m_Ns{0}, m_PN{0}, m_EC_DHr{}, m_valid{false}, m_size{0}{ // init valid to false and check during parsing if all is ok
				// make sure we have at least enough data to parse version<1 byte> || message type<1 byte> || curve Id<1 byte> || [x3dh init] || OPk flag without any ulterior checks on size
				if (header.size()<3 || header.size()<lime::double_ratchet_protocol::headerSize<Algo>(header[1])) {
					return; // the valid_flag is false
				}

				switch (header[0]) { // header[0] contains DR protocol version
					case lime::double_ratchet_protocol::DR_v01: { // version 0x01 of protocol, see in lime_utils for details
						if (header[2] != static_cast<uint8_t>(Algo::curveId())) return; // wrong curve in use, return with valid flag false
						uint8_t messageType = header[1];
						// Parse the message type byte
						if (messageType & static_cast<uint8_t>(lime::double_ratchet_protocol::DR_message_type::payload_direct_encryption_flag)) {
							m_payload_direct_encryption = true;
						} else {
							m_payload_direct_encryption = false;
						}
						if (messageType & static_cast<uint8_t>(lime::double_ratchet_protocol::DR_message_type::KEM_pk_index)) {
							m_havePkIndex = true;
						} else {
							m_havePkIndex = false;
						}

						m_size = lime::double_ratchet_protocol::headerSize<Algo>(header[1]); // headerSize is the size when no X3DH init is present
						size_t index = 3;
						if (messageType & static_cast<uint8_t>(lime::double_ratchet_protocol::DR_message_type::X3DH_init_flag)) {

							size_t x3dh_initMessageSize = lime::double_ratchet_protocol::X3DHinitSize<Algo>(header[3] == 1);
							m_size += x3dh_initMessageSize;
							index += x3dh_initMessageSize;
						}
						if (header.size() >=  m_size) { //header shall be actually longer because buffer pass is the whole message
							m_Ns = header[index]<<8|header[index+1];
							index += 2;
							m_PN = header[index]<<8|header[index+1];
							index += 2;

							if (m_havePkIndex) { // We have a EC key and KEM indexes
								m_EC_DHr.assign(header.cbegin()+index);
								index += m_EC_DHr.size();
								m_KEMDHrIndex.assign(header.cbegin()+index, header.cbegin()+index + lime::settings::DRPkIndexSize);
								index += lime::settings::DRPkIndexSize;
								m_KEMDHsIndex.assign(header.cbegin()+index, header.cbegin()+index + lime::settings::DRPkIndexSize);
							} else { // We have a whole DHs in this message, copy it
								std::copy_n(header.cbegin()+index, m_DHr.size(), m_DHr.begin());
							}
							m_valid = true;
						}
					}
					break;

					default: // just do nothing, we do not know this version of header, don't parse anything and leave its valid flag to false
					break;
				}
			 }

			~DRHeader() {};

			void dump(std::ostringstream &os, std::string indent="        ") const {
				if (m_valid) {}
					os<<std::endl<<indent<<"Ns: 0x"<<std::dec<<std::setw(4) << std::setfill('0') << m_Ns;
					os<<std::endl<<indent<<"PN: 0x"<<std::dec<<std::setw(4) << std::setfill('0') << m_PN;
					os<<std::endl<<indent<<"payload direct encryption: "<<m_payload_direct_encryption;
					os<<std::endl<<indent<<"PK KEM Index: "<<m_havePkIndex;
					if (m_havePkIndex) {
						os<<std::endl<<indent<<"EC Pk: ";
						hexStr(os, m_EC_DHr.data(),  m_EC_DHr.size());
						os<<std::endl<<indent<<"KEM Index DHr: ";
						hexStr(os, m_KEMDHrIndex.data(),  m_KEMDHrIndex.size());
						os<<std::endl<<indent<<"KEM Index DHs: ";
						hexStr(os, m_KEMDHsIndex.data(),  m_KEMDHsIndex.size());
					} else {
						os<<std::endl<<indent<<"EC Pk: ";
						hexStr(os, m_DHr.data(),  lime::X<Algo, lime::Xtype::publicKey>::ssize());
						os<<std::endl<<indent<<"KEM Pk: ";
						hexStr(os, m_DHr.data() + lime::X<Algo, lime::Xtype::publicKey>::ssize(),  K<Algo, lime::Ktype::publicKey>::ssize());
						os<<std::endl<<indent<<"KEM Ct: ";
						hexStr(os, m_DHr.data() + lime::X<Algo, lime::Xtype::publicKey>::ssize() + K<Algo, lime::Ktype::publicKey>::ssize(), K<Algo, lime::Ktype::cipherText>::ssize());
					}
			}
	 };
}// anonymous namespace for local functions/classes


	/**
	 * @brief a Double Rachet session, implements the DR interface.
	 *
	 * A session is associated to a local user and a peer device.
	 * It stores all the state variables described in Double Ratcher spec section 3.2 and provide encrypt/decrypt functions
	 *
	 * @tparam Curve	The elliptic curve to use: C255 or C448
	 */
	template <typename Curve>
	class DRi : public DR {
		public:
			/****************************************************************************/
			/* DRi class constructors: 3 cases                                          */
			/*     - sender's init : 2 versions: for regular DR or KEM added DR         */
			/*     - receiver's init                                                    */
			/*     - initialisation from session stored in local storage                */
			/****************************************************************************/
			/**
			 * @brief Create a new DR session for sending message. Match pseudo code for RatchetInitAlice in DR spec section 3.3
			 * We have a shared key and peer's public key
			 * for EC based only Double Ratchet
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
			template<typename Curve_ = Curve, std::enable_if_t<!std::is_base_of_v<genericKEM, Curve_>, bool> = true>
			DRi(std::shared_ptr<lime::Db> localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const ARrKey<Curve> &peerPublicKey, long int peerDid, const std::string &peerDeviceId, const DSA<Curve, lime::DSAtype::publicKey> &peerIk, long int selfDid, const std::vector<uint8_t> &X3DH_initMessage, std::shared_ptr<RNG> RNG_context)
			:m_ARKeys{peerPublicKey},
			m_forceKEMRatchet{false}, m_peerKEMPkAvailable{false},  m_peerHasSelfKEMPk{false},
			m_peerECPkAvailable{false}, m_KEMRatchetChainSize{0}, m_lastKEMRatchetEpoch(0),
			m_RK(SK),m_CKs{},m_CKr{},m_Ns(0),m_Nr(0),m_PN(0),m_sharedAD(AD),m_mkskipped{},
			m_RNG{RNG_context},m_dbSessionId{0},m_usedNr{0},m_usedDHid{0}, m_usedOPkId{0}, m_localStorage{localStorage},m_dirty{DRSessionDbStatus::dirty},m_peerDid{peerDid},m_peerDeviceId{},
			m_peerIk{},m_db_Uid{selfDid}, m_active_status{true}, m_X3DH_initMessage{X3DH_initMessage}
			{
				// generate a new self key pair
				auto DH = make_keyExchange<Curve>();
				DH->createKeyPair(m_RNG);

				// copy the peer public key into ECDH context
				DH->set_peerPublic(peerPublicKey.publicKey());

				// get self key pair from context
				m_ARKeys.setDHs(ARsKey<Curve>(DH->get_selfPublic(), DH->get_secret()));

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
			 * @brief Create a new DR session for sending message. Match pseudo code for RatchetInitAlice in DR spec section 3.3
			 * for KEM and EC based Double Ratchet
			 *
			 * @param[in]	localStorage		Local storage accessor to save DR session and perform mkskipped lookup
			 * @param[in]	SK			a 32 bytes shared secret established prior the session init (likely done using X3DH)
			 * @param[in]	AD			The associated data generated by X3DH protocol and permanently part of the DR session(see X3DH spec section 3.3 and lime doc section 5.4.3)
			 * @param[in]	peerPublicKey		the public key of message recipient (also obtained through X3DH, shall be peer SPk): holds DH and KEM peer public key provided by PQX3DH
			 * @param[in]	peerDid			Id used in local storage for this peer Device this session shall be attached to
			 * @param[in]	peerDeviceId		The peer Device Id this session is connected to. Ignored if peerDid is not 0
			 * @param[in]	peerIk			The Identity Key of the peer device this session is connected to. Ignored if peerDid is not 0
			 * @param[in]	selfDid			Id used in local storage for local user this session shall be attached to
			 * @param[in]	X3DH_initMessage	at session creation as sender we shall also store the X3DHInit message to be able to include it in all message until we got a response from peer
			 * @param[in]	RNG_context		A Random Number Generator context used for any rndom generation needed by this session
			 */
			template<typename Curve_ = Curve, std::enable_if_t<std::is_base_of_v<genericKEM, Curve_>, bool> = true>
			DRi(std::shared_ptr<lime::Db> localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const ARrKey<Curve> &peerPublicKey, long int peerDid, const std::string &peerDeviceId, const DSA<typename Curve::EC, lime::DSAtype::publicKey> &peerIk, long int selfDid, const std::vector<uint8_t> &X3DH_initMessage, std::shared_ptr<RNG> RNG_context)
			:m_ARKeys{peerPublicKey},
			m_forceKEMRatchet{false}, m_peerKEMPkAvailable{false},  m_peerHasSelfKEMPk{false},
			m_peerECPkAvailable{false}, m_KEMRatchetChainSize{0}, m_lastKEMRatchetEpoch(0),
			m_RK(SK),m_CKs{},m_CKr{},m_Ns(0),m_Nr(0),m_PN(0),m_sharedAD(AD),m_mkskipped{},
			m_RNG{RNG_context},m_dbSessionId{0},m_usedNr{0},m_usedDHid{0}, m_usedOPkId{0}, m_localStorage{localStorage},m_dirty{DRSessionDbStatus::dirty},m_peerDid{peerDid},m_peerDeviceId{},
			m_peerIk{},m_db_Uid{selfDid}, m_active_status{true}, m_X3DH_initMessage{X3DH_initMessage}
			{
				auto DH = make_keyExchange<typename Curve::EC>();
				auto KEMengine = make_KEM<typename Curve::KEM>();

				// generate a new self key pairs
				DH->createKeyPair(m_RNG);

				// Compute shared secrets
				DH->set_peerPublic(peerPublicKey.ECPublicKey());
				DH->computeSharedSecret();
				K<typename Curve::KEM, lime::Ktype::sharedSecret> KEMss{};
				K<typename Curve::KEM, lime::Ktype::cipherText> KEMct{};
				KEMengine->encaps(peerPublicKey.KEMPublicKey(), KEMct, KEMss);

				//  Derive the new sending chain key
				KEM_KDF_RK<Curve>(m_RK, m_CKs, DH->get_sharedSecret(), KEMss,
								  DH->get_selfPublic(), peerPublicKey.ECPublicKey(),
								  peerPublicKey.KEMPublicKey(), KEMct);

				// save self key pair into context
				Kpair<typename Curve::KEM> ARsKEMpair{};
				KEMengine->createKeyPair(ARsKEMpair);
				m_ARKeys.setDHs(ARsKey<Curve>(DH->get_selfPublic(), DH->get_secret(), ARsKEMpair.cpublicKey(), ARsKEMpair.cprivateKey(), KEMct));

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
			DRi(std::shared_ptr<lime::Db> localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const ARsKey<Curve> &selfKeyPair, long int peerDid, const std::string &peerDeviceId, const uint32_t OPk_id, const DSA<typename Curve::EC, lime::DSAtype::publicKey> &peerIk, long int selfDid, std::shared_ptr<RNG> RNG_context)
			:m_ARKeys{selfKeyPair},
			m_forceKEMRatchet{true}, m_peerKEMPkAvailable{true},  m_peerHasSelfKEMPk{false},
			m_peerECPkAvailable{true}, m_KEMRatchetChainSize{0}, m_lastKEMRatchetEpoch(0),
			m_RK(SK),m_CKs{},m_CKr{},m_Ns(0),m_Nr(0),m_PN(0),m_sharedAD(AD),m_mkskipped{},
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
			DRi(std::shared_ptr<lime::Db> localStorage, long sessionId, std::shared_ptr<RNG> RNG_context)
			:m_ARKeys{},
			m_forceKEMRatchet{false}, m_peerKEMPkAvailable{false},  m_peerHasSelfKEMPk{false},
			m_peerECPkAvailable{false}, m_KEMRatchetChainSize{0}, m_lastKEMRatchetEpoch(0),
			m_RK{},m_CKs{},m_CKr{},m_Ns(0),m_Nr(0),m_PN(0),m_sharedAD{},m_mkskipped{},
			m_RNG{RNG_context},m_dbSessionId{sessionId},m_usedNr{0},m_usedDHid{0}, m_usedOPkId{0}, m_localStorage{localStorage},m_dirty{DRSessionDbStatus::clean},m_peerDid{0},m_peerDeviceId{},
			m_peerIk{},m_db_Uid{0},	m_active_status{false}, m_X3DH_initMessage{}
			{
				m_ARKeys.setValid(session_load());
			}

			DRi() = delete; // make sure the Double Ratchet is not initialised without parameters
			DRi(DRi<Curve> &a) = delete; // can't copy a session, force usage of shared pointers
			DRi<Curve> &operator=(DRi<Curve> &a) = delete; // can't copy a session
			~DRi() {};

			/* Implement the DR interface */
			void ratchetEncrypt(const std::vector<uint8_t> &plaintext, std::vector<uint8_t> &&AD, std::vector<uint8_t> &ciphertext, const bool payloadDirectEncryption) override;
			bool ratchetDecrypt(const std::vector<uint8_t> &cipherText, const std::vector<uint8_t> &AD, std::vector<uint8_t> &plaintext, const bool payloadDirectEncryption) override;
			/// return the session's local storage id
			long int dbSessionId(void) const override {return m_dbSessionId;};
			/// return the current status of session
			bool isActive(void) const override {return m_active_status;}

		private:
			/* State variables for Double Ratchet, see Double Ratchet spec section 3.2 for details */
			ARKeys<Curve> m_ARKeys; // Asymmetric Ratchet keys
			bool m_forceKEMRatchet; // flag set to true at session creation on reception to force a KEM ratchet on the first response
			bool m_peerKEMPkAvailable; // true : the KEM peer Public key was not yet consumed to update sending chain
			bool m_peerHasSelfKEMPk; // true: our correspondant have our current public key
			bool m_peerECPkAvailable; // true : the EC peer Public key was not yet consumed to update sending chain
			uint32_t m_KEMRatchetChainSize; // How many messages were exchanged since the last KEM Ratchet
			int64_t m_lastKEMRatchetEpoch; // timestamp storing the last asymmetric receiving ratchet execution (as unixepoch)
			DRChainKey m_RK; // 32 bytes root key
			DRChainKey m_CKs; // 32 bytes key chain for sending
			DRChainKey m_CKr; // 32 bytes key chain for receiving
			uint16_t m_Ns,m_Nr; // Message index in sending and receiving chain
			uint16_t m_PN; // Number of messages in previous sending chain
			SharedADBuffer m_sharedAD; // Associated Data derived from self and peer device Identity key, set once at session creation, given by X3DH
			std::vector<lime::ReceiverKeyChain<Curve>> m_mkskipped; // list of skipped message indexed by DH receiver public key and Nr, store MK generated during on-going decrypt, lookup is done directly in DB.

			/* helpers variables */
			std::shared_ptr<RNG> m_RNG; // Random Number Generator context
			long int m_dbSessionId; // used to store row id from Database Storage
			uint16_t m_usedNr; // store the index of message key used for decryption if it came from mkskipped db
			long m_usedDHid; // store the index of DHr message key used for decryption if it came from mkskipped db(not zero only if used)
			uint32_t m_usedOPkId; // when the session is created on receiver side, store the OPk id used so we can remove it from local storage when saving session for the first time.
			std::shared_ptr<lime::Db> m_localStorage; // enable access to the database holding sessions and skipped message keys
			DRSessionDbStatus m_dirty; // status of the object regarding its instance in local storage, could be: clean, dirty_encrypt, dirty_decrypt or dirty
			long int m_peerDid; // used during session creation only to hold the peer device id in DB as we need it to insert the session in local Storage
			std::string m_peerDeviceId; // used during session creation only, if the deviceId is not yet in local storage, to hold the peer device Id so we can insert it in DB when session is saved for the first time
			DSA<typename Curve::EC, lime::DSAtype::publicKey> m_peerIk; // used during session creation only, if the deviceId is not yet in local storage, to hold the peer device Ik so we can insert it in DB when session is saved for the first time
			long int m_db_Uid; // used to link session to a local device Id
			bool m_active_status; // current status of this session, true if it is the active one, false if it is stale
			std::vector<uint8_t> m_X3DH_initMessage; // store the X3DH init message to be able to prepend it to any message until we got a first response from peer so we're sure he was able to init the session on his side

			/*helpers functions */
			void skipMessageKeys(const uint16_t until, const int limit); /* check if we skipped some messages in current receiving chain, generate and store in session intermediate message keys */
			/* local storage related */
			int DHrStatusToInt(void); /* compute the DHr status Integer stored in DB aggregating information about Peer's and Self public key status */
			void IntToDHrStatus(int DHrStatus); /* set information related to Peer's and Self pk into the session from the int stored in DB */
			bool session_save(bool commit=true); /* save/update session in database : updated component depends m_dirty value, when commit is true, commit transaction in DB */
			bool session_load(); /* load session from database */
			bool trySkippedMessageKeys(const uint16_t Nr, const std::vector<uint8_t> &DHrIndex, DRMKey &MK); /* check in DB if we have a message key matching public DH and Ns */

			/**
			 * @brief perform an Asymmetric Ratchet based on Diffie-Hellman as described in DR spec section 3.5
			 * This function only performs the receiving part: execute it when we receive a new peer public key in a DR message header
			 *
			 * @param[in] headerDH	The peer public key provided in the message header
			 */
			template<typename Curve_ = Curve, std::enable_if_t<!std::is_base_of_v<genericKEM, Curve_>, bool> = true>
			void AsymmetricRatchetReceiving(const DRHeader<Curve> &header) {
				// reinit counters
				m_Nr=0;
				m_KEMRatchetChainSize = 0; // This one is not used by non KEM ratchet but always defined in DRi object so reset it to avoid overflow

				// this is our new DHr
				m_ARKeys.setDHr(header.DHr());
				m_peerECPkAvailable = true; // this is a fresh peer key, no sending ratchet performed with it yet

				// compute shared secret with new peer public and current self secret
				auto DH = make_keyExchange<typename Curve::EC>();
				DH->set_peerPublic(m_ARKeys.cgetDHr().publicKey());
				DH->set_selfPublic(m_ARKeys.getDHs().publicKey()); // TODO: Do we need to copy self public key in context ?
				DH->set_secret(m_ARKeys.getDHs().privateKey());

				//  Derive the new receiving chain key
				DH->computeSharedSecret();
				KDF_RK<Curve>(m_RK, m_CKr, DH->get_sharedSecret());

				// modified the DR session, not in sync anymore with local storage
				m_dirty = DRSessionDbStatus::dirty_ratchet_receiving;
			}
			/**
			 * @brief perform an Asymmetric Ratchet based on Diffie-Hellman as described in DR spec section 3.5
			 * This function only performs the sending part: execute it when we want to send a message with asymmetric ratchet
			 */
			template<typename Curve_ = Curve, std::enable_if_t<!std::is_base_of_v<genericKEM, Curve_>, bool> = true>
			void AsymmetricRatchetSending() {
				// reinit counters
				m_PN=m_Ns;
				m_Ns=0;
				m_KEMRatchetChainSize = 0; // This one is not used by non KEM ratchet but always defined in DRi object so reset it to avoid overflow

				auto DH = make_keyExchange<typename Curve::EC>();
				// generate a new self key pair
				DH->createKeyPair(m_RNG);

				// compute shared secret with new self and current peer public
				DH->set_peerPublic(m_ARKeys.cgetDHr().publicKey());
				DH->computeSharedSecret();
				KDF_RK<Curve>(m_RK, m_CKs, DH->get_sharedSecret());

				// set self key pair in context
				m_ARKeys.setDHs(ARsKey<Curve>(DH->get_selfPublic(), DH->get_secret()));

				// modified the DR session, not in sync anymore with local storage
				m_dirty = DRSessionDbStatus::dirty_ratchet_sending;
				m_peerECPkAvailable = false; // make sure we will not make another ratchet with this key
			}
			/**
			 * @brief perform an Asymmetric Ratchet with KEM on reception of peer's new public key
			 *
			 * - for the DH part: exact same principle as decribed in DR spec section 3.5
			 * - KEM part: depends on header, if there is a KEM public key/cipher text that we do not know, do it
			 *   * a cipher text encapsulating a secret we can decapsulate using our self KEM private key -> needed to update the receiving key chain
			 *   * peer's header holds a public key we use to encapsulate a fresh secret -> store it and use it when we update the sending key chain
			 *
			 * @param[in] headerDH	The peer public keys provided in the message header: DH pk || KEM pk || KEM ct
			 */
			template<typename Curve_ = Curve, std::enable_if_t<std::is_base_of_v<genericKEM, Curve_>, bool> = true>
			void AsymmetricRatchetReceiving(const DRHeader<Curve> &header) {
				// reinit counters
				m_Nr=0;

				if (header.havePKIndex()) { // if this header holds only an EC Pk and KEM keys indexes
					// set only a new EC DHr
					m_ARKeys.getDHr().setECPk(header.ECDHr());
				} else {
					// set the whole DHr
					m_ARKeys.setDHr(header.DHr());
					m_peerKEMPkAvailable = true; // this is a fresh peer key, no sending ratchet performed with it yet
					m_peerHasSelfKEMPk = true; // peer also have our current public key - as it was used to perform an encapsulation
					m_KEMRatchetChainSize = 0;
				}
				m_peerECPkAvailable = true; // this is a fresh peer key, no sending ratchet performed with it yet

				// compute shared secret with new peer EC public and current self secret
				auto DH = make_keyExchange<typename Curve::EC>();
				DH->set_peerPublic(m_ARKeys.cgetDHr().ECPublicKey());
				DH->set_selfPublic(m_ARKeys.cgetDHs().ECPublicKey());
				DH->set_secret(m_ARKeys.getDHs().ECPrivateKey());
				DH->computeSharedSecret();

				// EC ratchet only
				if (header.havePKIndex()) {
					KDF_RK<typename Curve::EC>(m_RK, m_CKr, DH->get_sharedSecret());
					// modified the DR session, not in sync anymore with local storage
					m_dirty = DRSessionDbStatus::dirty_ratchet_receiving;
				}else  { // We must also perform a KEM ratchet
					auto KEMengine = make_KEM<typename Curve::KEM>();
					K<typename Curve::KEM, lime::Ktype::sharedSecret> KEMss{};
					KEMengine->decaps(m_ARKeys.getDHs().KEMPrivateKey(), m_ARKeys.getDHr().KEMCipherText(), KEMss);
					//  Derive the new receiving chain key
					KEM_KDF_RK<Curve>(m_RK, m_CKr, DH->get_sharedSecret(), KEMss,
								  m_ARKeys.cgetDHr().ECPublicKey(), m_ARKeys.cgetDHs().ECPublicKey(),
								  m_ARKeys.cgetDHs().KEMPublicKey(), m_ARKeys.cgetDHr().KEMCipherText());
					// modified the DR session, not in sync anymore with local storage
					m_dirty = DRSessionDbStatus::dirty_kem_ratchet_receiving;
				}
			}

			/** EC/KEM based asymmetric ratchet sending
			 * We may perform a simple EC ratchet or an EC+KEM ratchet
			 * Simple EC ratchet is the default (as its cost - in terms of header overhead) is cheap
			 * EC/KEM overhead is more of a burden so use it less often
			 */
			template<typename Curve_ = Curve, std::enable_if_t<std::is_base_of_v<genericKEM, Curve_>, bool> = true>
			void AsymmetricRatchetSending() {
				// Shall we perform a KEM ratchet
				bool KEMRatchet = false;
				if (m_peerKEMPkAvailable) { // Can we do that?
					bctoolboxTimeSpec currentUTCtime;
					bctbx_get_utc_cur_time(&currentUTCtime);
					if ( (m_KEMRatchetChainSize > lime::settings::KEMRatchetChainSize) // if the chain is long enough
						|| ((currentUTCtime.tv_sec - m_lastKEMRatchetEpoch)>lime::settings::maxKEMRatchetChainPeriod)  // or the last KEM ratchet is old enough
						|| m_forceKEMRatchet) { // or we force the asymmetric ratchet (first encryption after a session init on receiver side)
							KEMRatchet = true;
					}
				}

				// reinit counters
				m_PN=m_Ns;
				m_Ns=0;

				// generate a new shared secrets: new key pair for EC
				auto DH = make_keyExchange<typename Curve::EC>();
				DH->set_peerPublic(m_ARKeys.cgetDHr().ECPublicKey());
				DH->createKeyPair(m_RNG);
				DH->computeSharedSecret();
				if (KEMRatchet) {
					auto KEMengine = make_KEM<typename Curve::KEM>();
					// encapsulate a new shared secret using peer's public key
					K<typename Curve::KEM, lime::Ktype::sharedSecret> KEMss{};
					K<typename Curve::KEM, lime::Ktype::cipherText> KEMct{};
					KEMengine->encaps(m_ARKeys.cgetDHr().KEMPublicKey(), KEMct, KEMss);

					//  Derive the new sending chain key
					KEM_KDF_RK<Curve>(m_RK, m_CKs, DH->get_sharedSecret(), KEMss,
								  DH->get_selfPublic(), m_ARKeys.cgetDHr().ECPublicKey(),
								  m_ARKeys.getDHr().KEMPublicKey(), KEMct);
					// Generate a new key pair for KEM
					Kpair<typename Curve::KEM> ARsKEMpair{};
					KEMengine->createKeyPair(ARsKEMpair);
					// save self new key pairs in context
					m_ARKeys.setDHs(ARsKey<Curve>(DH->get_selfPublic(), DH->get_secret(), ARsKEMpair.cpublicKey(), ARsKEMpair.cprivateKey(), KEMct));
					m_peerHasSelfKEMPk = false;
					m_peerKEMPkAvailable = false; // make sure we will not make another ratchet with this key
					m_forceKEMRatchet = false; // we are making a ratchet so make sure to clear the force ratchet flag used only to force it once after session creation on receiver side
				} else {
					KDF_RK<typename Curve::EC>(m_RK, m_CKs, DH->get_sharedSecret());
					// save self new key pairs in context
					m_ARKeys.getDHs().setEC(DH->get_selfPublic(), DH->get_secret());
				}

				// modified the DR session, not in sync anymore with local storage
				m_dirty = DRSessionDbStatus::dirty_ratchet_sending;
				m_peerECPkAvailable = false; // make sure we will not make another ratchet with this key
			}

			/**
			* @brief Build a header string from needed info
			* version for EC key only: asymmetric ratchet public keys is always present
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
			* @param[in]	payloadDirectEncryption		Set the Payload Direct Encryption flag in header
			*/
			template<typename Curve_ = Curve, std::enable_if_t<!std::is_base_of_v<genericKEM, Curve_>, bool> = true>
			void writeDRheader(std::vector<uint8_t> &header, const bool payloadDirectEncryption) const noexcept {
				header.assign(1, static_cast<uint8_t>(double_ratchet_protocol::DR_v01));
				uint8_t messageType = 0;
				if (payloadDirectEncryption) { // if requested, turn the payload direct encryption flag on
					messageType |= static_cast<uint8_t>(lime::double_ratchet_protocol::DR_message_type::payload_direct_encryption_flag); // turn on the flag
				}

				if (m_X3DH_initMessage.size()>0) { // we do have an X3DH init message to insert in the header
					messageType |= static_cast<uint8_t>(lime::double_ratchet_protocol::DR_message_type::X3DH_init_flag); // turn on the flag
					header.push_back(messageType);
					header.push_back(static_cast<uint8_t>(Curve::curveId()));
					header.insert(header.end(), m_X3DH_initMessage.cbegin(), m_X3DH_initMessage.cend());
				} else {
					messageType &= ~static_cast<uint8_t>(lime::double_ratchet_protocol::DR_message_type::X3DH_init_flag); // be sure to have this flag turned off
					header.push_back(messageType);
					header.push_back(static_cast<uint8_t>(Curve::curveId()));
				}
				header.push_back((uint8_t)((m_Ns>>8)&0xFF));
				header.push_back((uint8_t)(m_Ns&0xFF));
				header.push_back((uint8_t)((m_PN>>8)&0xFF));
				header.push_back((uint8_t)(m_PN&0xFF));
				const auto DHs = m_ARKeys.serializePublicDHs();
				header.insert(header.end(), DHs.cbegin(), DHs.cend());
			}

			/**
			* @brief Build a header string from needed info
			* version for EC/KEM key only, we can have
			* - asymmetric ratchet public keys (both EC and KEM)
			* - EC public key and Self KEM index + Peer KEM index
			*
			*	header is:
			*
			*	Protocol Version Number<1 byte> ||\n
			*	Message Type <1 byte> ||\n
			*	curveId <1 byte> ||\n
			*	[X3DH Init message < variable >] ||\n
			*	Ns<2 bytes> ||\n
			*	PN<2 bytes> ||\n
			*	ECDHs<...>
			*   Self KEM index <12 bytes> || Peer KEM index <12 bytes>
			*
			* @param[in]	payloadDirectEncryption		Set the Payload Direct Encryption flag in header
			*/
			template<typename Curve_ = Curve, std::enable_if_t<std::is_base_of_v<genericKEM, Curve_>, bool> = true>
			void writeDRheader(std::vector<uint8_t> &header, const bool payloadDirectEncryption) const noexcept {
				header.assign(1, static_cast<uint8_t>(double_ratchet_protocol::DR_v01));
				uint8_t messageType = 0;
				if (payloadDirectEncryption) { // if requested, turn the payload direct encryption flag on
					messageType |= static_cast<uint8_t>(lime::double_ratchet_protocol::DR_message_type::payload_direct_encryption_flag); // turn on the flag
				}
				if (m_peerHasSelfKEMPk) { // peer already get our KEM Pk
					// No KEM asymmetric ratchet public key but index only
					messageType |= static_cast<uint8_t>(lime::double_ratchet_protocol::DR_message_type::KEM_pk_index); // turn on the flag
				}

				if (m_X3DH_initMessage.size()>0) { // we do have an X3DH init message to insert in the header
					messageType |= static_cast<uint8_t>(lime::double_ratchet_protocol::DR_message_type::X3DH_init_flag); // turn on the flag
					header.push_back(messageType);
					header.push_back(static_cast<uint8_t>(Curve::curveId()));
					header.insert(header.end(), m_X3DH_initMessage.cbegin(), m_X3DH_initMessage.cend());
				} else {
					messageType &= ~static_cast<uint8_t>(lime::double_ratchet_protocol::DR_message_type::X3DH_init_flag); // be sure to have this flag turned off
					header.push_back(messageType);
					header.push_back(static_cast<uint8_t>(Curve::curveId()));
				}
				header.push_back((uint8_t)((m_Ns>>8)&0xFF));
				header.push_back((uint8_t)(m_Ns&0xFF));
				header.push_back((uint8_t)((m_PN>>8)&0xFF));
				header.push_back((uint8_t)(m_PN&0xFF));
				if (m_peerHasSelfKEMPk) { // peer already get our KEM Pk
					// insert EC public part
					const auto ECDHs = m_ARKeys.cgetDHs().serializeECPublic();
					header.insert(header.end(), ECDHs.cbegin(), ECDHs.cend());
					// insert self KEM key index
					const auto KEMDHsIndex = m_ARKeys.cgetDHs().getKEMIndex();
					header.insert(header.end(), KEMDHsIndex.cbegin(), KEMDHsIndex.cend());
					// insert peer KEM key index
					const auto KEMDHrIndex = m_ARKeys.cgetDHr().getKEMIndex();
					header.insert(header.end(), KEMDHrIndex.cbegin(), KEMDHrIndex.cend());
				} else { // insert the whole DHs (including KEM part)
					const auto DHs = m_ARKeys.serializePublicDHs();
					header.insert(header.end(), DHs.cbegin(), DHs.cend());
				}
			}
	};

	/****************************************************************************/
	/* DRi public member functions - DR interface implementation                */
	/****************************************************************************/
	/**
	 * @brief Encrypt using the double-ratchet algorithm.
	 *
	 * @param[in]	plaintext			the input to be encrypted, may actually be a 32 bytes buffer holding the seed used to generate key+IV for a AES-GCM encryption to the actual message
	 * @param[in]	AD				Associated Data, this buffer shall hold: source GRUU<...> || recipient GRUU<...> || [ actual message AEAD auth tag OR recipient User Id]
	 * @param[out]	ciphertext			buffer holding the header, cipher text and auth tag, shall contain the key and IV used to cipher the actual message, auth tag applies on AD || header
	 * @param[in]	payloadDirectEncryption		A flag to set in message header: set when having payload in the DR message
	 */
	template <typename Curve>
	void DRi<Curve>::ratchetEncrypt(const std::vector<uint8_t> &plaintext, std::vector<uint8_t> &&AD, std::vector<uint8_t> &ciphertext, const bool payloadDirectEncryption) {
		m_dirty = DRSessionDbStatus::dirty_encrypt; // we're about to modify this session, it won't be in sync anymore with local storage
		// Shall we perform an asymmetric ratchet step? If there is at least an EC public key available, yes
		if (m_peerECPkAvailable) {
			AsymmetricRatchetSending();
		}

		// chain key derivation(also compute message key)
		DRMKey MK;
		KDF_CK<Curve>(m_CKs, MK, m_Ns);

		ciphertext.clear();
		writeDRheader(ciphertext, payloadDirectEncryption);
		auto headerSize = ciphertext.size(); // cipher text holds only the DR header for now

		// increment current sending chain message index
		m_Ns++;
		m_KEMRatchetChainSize++;

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
	 * @param[in]	ciphertext			Input to be decrypted, is likely to be a 32 bytes vector holding the crypted version of a random seed
	 * @param[in]	AD				Associated data authenticated along the encryption (initial session AD and DR message header are append to it)
	 * @param[out]	plaintext			Decrypted output
	 * @param[in]	payloadDirectEncryption		A flag to enforce checking on message type: when set we expect to get payload in the message(so message header matching flag must be set)
	 *
	 * @return	true on success
	 */
	template <typename Curve>
	bool DRi<Curve>::ratchetDecrypt(const std::vector<uint8_t> &ciphertext,const std::vector<uint8_t> &AD, std::vector<uint8_t> &plaintext, const bool payloadDirectEncryption) {
		// parse header
		DRHeader<Curve> header{ciphertext};
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
		if (!m_ARKeys.getValid()) { // it's the first message arriving after the initialisation of the chain in receiver mode, we have no existing history in this chain
			AsymmetricRatchetReceiving(header); // just perform the DH ratchet step
			m_ARKeys.setValid(true);
		} else {
			// Should we look for skipped keys:
			bool lookForStoredMessageKeys = true;
			bool asymmetricRatchet = false;
			// - if the incoming message is on the current receiving chain
			if ( header.isDHr(m_ARKeys.cgetDHr())) {
				// - and the Ns is greater or equal to the current Nr
				if (header.Ns()>=m_Nr) {
					// Then the message key can be computed and is not on storage -> do not search for it
					lookForStoredMessageKeys = false;
				}
			} else { // The header receiving chain is not the one we're onto. So if we do not find skipped key, we must perform an asymmetric ratchet
				asymmetricRatchet = true;
			}
			// Can we confirm that peer got our current public key already (so we may stop sending it)
			if (header.isDHsIndex(m_ARKeys.cgetDHs())) {
				m_peerHasSelfKEMPk = true;
			}

			if (lookForStoredMessageKeys) {
				// check stored message keys(lime_tester::curveId(curve))
				auto foundSkippedKey = trySkippedMessageKeys(header.Ns(), header.getDHrIndex(), MK);

				// Remove this check on version 5.5.
				// for retrocompatibility whith old deployments with curve C25519 or C448, we must also check for skipped message keys indexed directly by the key itself
				// Worst case we may loose skipped message keys
				if (!foundSkippedKey && !header.havePKIndex() && (Curve::curveId() == lime::CurveId::c25519 || Curve::curveId() == lime::CurveId::c448)) {
					auto DHr = std::make_shared<lime::ARrKey<Curve>>(header.DHr());
					std::vector<uint8_t> vDHr{header.DHr().cbegin(), header.DHr().cend()};
					foundSkippedKey = trySkippedMessageKeys(header.Ns(), vDHr, MK);
				}

				if (foundSkippedKey) {
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
						LIME_LOGE<<"Decryption fail: found a matching skipped key in local db but unable to decrypt with it";
						return false;
					}
				}
			}
			// if header DH public key != current stored peer public DH key: we must perform a DH ratchet
			if (asymmetricRatchet) {
				maxAllowedDerivation -= header.PN()-m_Nr; /* we must derive headerPN-Nr keys, remove this from the count of our allowedDerivation number */
				skipMessageKeys(header.PN(), lime::settings::maxMessageSkip-header.Ns()); // we must keep header.Ns derivations available for the next chain
				AsymmetricRatchetReceiving(header);
			}
		}

		// in the derivation limit we remove the derivation done in the previous DH rachet key chain
		skipMessageKeys(header.Ns(), maxAllowedDerivation);

		// generate key material for decryption(and derive Chain key)
		KDF_CK<Curve>(m_CKr, MK, m_Nr);

		m_Nr++;
		m_KEMRatchetChainSize++;

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

	/****************************************************************************/
	/* DRi private member functions                                             */
	/****************************************************************************/
	/**
	 * Status on peer's public key in the DR session, as stored in DB in a 4 bytes integer
	 * mapping:
	 *                       byte 3 | byte 2 | byte 1                                           | byte 0
	 *   <Total number of message sent/received - or skipped -since last KEM ratchet>             Flags as detailed below
	 * The total number of message should never increase more than maxKEMRatchetChainSize the peer in position of performing a KEM ratchet
	 * is encrypting message or maxSendingChain if he is not.

	 * Flags bitmap :
	 * -- 0  force KEM ratchet ASAP (set when a session is create on receiver side to force KEM ratchet at first response)
	 * -- 1  KEM peer pk available locally
	 * -- 2  KEM self pk known by peer
	 * -- 3  EC peer pk available locally
	 */
	namespace{
		enum class DHrStatusBitMap : uint32_t {
			forceKEMRatchet = 0x00000001,
			peerKEMPkAvailable = 0x00000002,
			peerHasSelfKEMPk = 0x00000004,
			peerECPkAvailable = 0x00000008,
			KEMRatchetChainSize = 0x7FFFFF00
		};
	}
	template <typename Curve>
	int DRi<Curve>::DHrStatusToInt(void) {
		int ret = 0;
		if (m_forceKEMRatchet) ret |= static_cast<uint32_t>(DHrStatusBitMap::forceKEMRatchet);
		if (m_peerKEMPkAvailable) ret |= static_cast<uint32_t>(DHrStatusBitMap::peerKEMPkAvailable);
		if (m_peerHasSelfKEMPk) ret |= static_cast<uint32_t>(DHrStatusBitMap::peerHasSelfKEMPk);
		if (m_peerECPkAvailable) ret |= static_cast<uint32_t>(DHrStatusBitMap::peerECPkAvailable);
		ret |= (m_KEMRatchetChainSize<<8)&static_cast<uint32_t>(DHrStatusBitMap::KEMRatchetChainSize);
		return ret;
	}
	template <typename Curve>
	void DRi<Curve>::IntToDHrStatus(int DHrStatus) {
		m_forceKEMRatchet = (DHrStatus & static_cast<uint32_t>(DHrStatusBitMap::forceKEMRatchet)) != 0;
		m_peerKEMPkAvailable = (DHrStatus & static_cast<uint32_t>(DHrStatusBitMap::peerKEMPkAvailable)) != 0;
		m_peerHasSelfKEMPk = (DHrStatus & static_cast<uint32_t>(DHrStatusBitMap::peerHasSelfKEMPk)) != 0;
		m_peerECPkAvailable = (DHrStatus & static_cast<uint32_t>(DHrStatusBitMap::peerECPkAvailable)) != 0;
		m_KEMRatchetChainSize = (DHrStatus & static_cast<uint32_t>(DHrStatusBitMap::KEMRatchetChainSize))>>8;
	}

	/**
	 * @brief Save a session to the local storage
	 *
	 * @param[in]	commit		when true, the modification to local storage is commited
	 * 				when false, allow to perform multiple session saving in a single transaction
	 */
	template <typename Curve>
	bool DRi<Curve>::session_save(bool commit) { // commit default to true
		std::lock_guard<std::recursive_mutex> lock(*(m_localStorage->m_db_mutex));

		try {
			if (commit) {
				// open transaction
				m_localStorage->start_transaction();
			}

			// shall we try to insert or update?
			bool MSk_DHr_Clean = false; // flag use to signal the need for late cleaning in DR_MSk_DHr table
			if (m_dbSessionId==0) { // We have no id for this session row, we shall insert a new one
				int DHrStatusInt = DHrStatusToInt();

				// Build blobs from DR session
				blob DHr(m_localStorage->sql);
				auto mDHr = m_ARKeys.serializeDHr();
				DHr.write(0, (char *)(mDHr.data()), mDHr.size());
				blob DHs(m_localStorage->sql);
				auto mDHs = m_ARKeys.serializeDHs();
				DHs.write(0, (char *)(mDHs.data()), mDHs.size());
				blob RK(m_localStorage->sql);
				RK.write(0, (char *)(m_RK.data()), m_RK.size());
				blob CKs(m_localStorage->sql);
				CKs.write(0, (char *)(m_CKs.data()), m_CKs.size());
				blob CKr(m_localStorage->sql);
				CKr.write(0, (char *)(m_CKr.data()), m_CKr.size());
				/* this one is written in base only at creation and never updated again */
				blob AD(m_localStorage->sql);
				AD.write(0, (char *)(m_sharedAD.data()), m_sharedAD.size());

				// Check if we have a peer device already in storage
				if (m_peerDid == 0) { // no : we must insert it(failure will result in exception being thrown, let it flow up then)
					m_peerDid = m_localStorage->store_peerDevice(m_peerDeviceId, m_peerIk);
				} else {
					// make sure we have no other session active with this pair local,peer DiD
					m_localStorage->sql<<"UPDATE DR_sessions SET Status = 0, timeStamp = CURRENT_TIMESTAMP WHERE Did = :Did AND Uid = :Uid", use(m_peerDid), use(m_db_Uid);
				}

				if (m_X3DH_initMessage.size()>0) {
					blob X3DH_initMessage(m_localStorage->sql);
					X3DH_initMessage.write(0, (char *)(m_X3DH_initMessage.data()), m_X3DH_initMessage.size());
					m_localStorage->sql<<"INSERT INTO DR_sessions(Ns,Nr,PN,DHr,DHrStatus,DHs,RK,CKs,CKr,AD,Did,Uid,X3DHInit) VALUES(:Ns,:Nr,:PN,:DHr,:DHrStatus,:DHs,:RK,:CKs,:CKr,:AD,:Did,:Uid,:X3DHinit);", use(m_Ns), use(m_Nr), use(m_PN), use(DHr), use(DHrStatusInt), use(DHs), use(RK), use(CKs), use(CKr), use(AD), use(m_peerDid), use(m_db_Uid), use(X3DH_initMessage);
				} else {
					m_localStorage->sql<<"INSERT INTO DR_sessions(Ns,Nr,PN,DHr,DHrStatus,DHs,RK,CKs,CKr,AD,Did,Uid) VALUES(:Ns,:Nr,:PN,:DHr,:DHrStatus,:DHs,:RK,:CKs,:CKr,:AD,:Did,:Uid);", use(m_Ns), use(m_Nr), use(m_PN), use(DHr), use(DHrStatusInt), use(DHs), use(RK), use(CKs), use(CKr), use(AD), use(m_peerDid), use(m_db_Uid);
				}
				// update session content with current timeStamp to reflect modifications in DB
				bctoolboxTimeSpec currentUTCtime;
				bctbx_get_utc_cur_time(&currentUTCtime);
				m_lastKEMRatchetEpoch = currentUTCtime.tv_sec;

				// if insert went well we shall be able to retrieve the last insert id to save it in the Session object
				/*** WARNING: unportable section of code, works only with sqlite3 backend ***/
				m_localStorage->sql<<"select last_insert_rowid()",into(m_dbSessionId);
				/*** above could should work but it doesn't, consistently return false from .get_last_insert_id... ***/
				/*if (!(sql.get_last_insert_id("DR_sessions", m_dbSessionId))) {
					throw;
				} */

				// At session creation, we may have to delete an OPk from storage
				if (m_usedOPkId != 0) {
					m_localStorage->sql<<"DELETE FROM X3DH_OPK WHERE Uid = :Uid AND OPKid = :OPk_id;", use(m_db_Uid), use(m_usedOPkId);
					m_usedOPkId = 0;
				}
			} else { // we have an id, it shall already be in the db
				// Update an existing row
				switch (m_dirty) {
					case DRSessionDbStatus::dirty: // dirty case shall actually never occurs as a dirty is set only at creation not loading, first save is processed above
					case DRSessionDbStatus::dirty_ratchet_receiving: // ratchet&decrypt
					{
						// make sure we have no other session active with this pair local,peer DiD
						if (m_active_status == false) {
							m_localStorage->sql<<"UPDATE DR_sessions SET Status = 0, timeStamp = CURRENT_TIMESTAMP WHERE Did = :Did AND Uid = :Uid", use(m_peerDid), use(m_db_Uid);
							m_active_status = true;
						}

						int DHrStatusInt = DHrStatusToInt();

						// Build blobs from DR session
						blob DHr(m_localStorage->sql);
						auto mDHr = m_ARKeys.serializeDHr();
						DHr.write(0, (char *)(mDHr.data()), mDHr.size());
						blob RK(m_localStorage->sql);
						RK.write(0, (char *)(m_RK.data()), m_RK.size());
						blob CKr(m_localStorage->sql);
						CKr.write(0, (char *)(m_CKr.data()), m_CKr.size());

						m_localStorage->sql<<"UPDATE DR_sessions SET Nr= :Nr, DHr= :DHr, DHrStatus= :DHrStatus, RK= :RK, CKr= :CKr, Status = 1,  X3DHInit = NULL WHERE sessionId = :sessionId;", use(m_Nr), use(DHr), use(DHrStatusInt), use(RK), use(CKr), use(m_dbSessionId);
					}
						break;
					case DRSessionDbStatus::dirty_kem_ratchet_receiving: // kem ratchet&decrypt
					{   // Same as EC only ratchet, but we also update the last Kem ratchet time in DB
						// make sure we have no other session active with this pair local,peer DiD
						if (m_active_status == false) {
							m_localStorage->sql<<"UPDATE DR_sessions SET Status = 0, timeStamp = CURRENT_TIMESTAMP WHERE Did = :Did AND Uid = :Uid", use(m_peerDid), use(m_db_Uid);
							m_active_status = true;
						}

						int DHrStatusInt = DHrStatusToInt();

						// Build blobs from DR session
						blob DHr(m_localStorage->sql);
						auto mDHr = m_ARKeys.serializeDHr();
						DHr.write(0, (char *)(mDHr.data()), mDHr.size());
						blob RK(m_localStorage->sql);
						RK.write(0, (char *)(m_RK.data()), m_RK.size());
						blob CKr(m_localStorage->sql);
						CKr.write(0, (char *)(m_CKr.data()), m_CKr.size());

						m_localStorage->sql<<"UPDATE DR_sessions SET Nr= :Nr, DHr= :DHr, DHrStatus= :DHrStatus, RK= :RK, CKr= :CKr, Status = 1,  X3DHInit = NULL, timeStamp = CURRENT_TIMESTAMP WHERE sessionId = :sessionId;", use(m_Nr), use(DHr), use(DHrStatusInt), use(RK), use(CKr), use(m_dbSessionId);

						// update session content with current timeStamp to reflect modifications in DB
						bctoolboxTimeSpec currentUTCtime;
						bctbx_get_utc_cur_time(&currentUTCtime);
						m_lastKEMRatchetEpoch = currentUTCtime.tv_sec;
					}
						break;
					case DRSessionDbStatus::dirty_ratchet_sending: // ratchet&encrypt
					{
						int DHrStatusInt = DHrStatusToInt();

						// Build blobs from DR session
						blob DHs(m_localStorage->sql);
						auto mDHs = m_ARKeys.serializeDHs();
						DHs.write(0, (char *)(mDHs.data()), mDHs.size());
						blob RK(m_localStorage->sql);
						RK.write(0, (char *)(m_RK.data()), m_RK.size());
						blob CKs(m_localStorage->sql);
						CKs.write(0, (char *)(m_CKs.data()), m_CKs.size());

						m_localStorage->sql<<"UPDATE DR_sessions SET Ns= :Ns, PN= :PN, DHrStatus= :DHrStatus, DHs= :DHs, RK= :RK, CKs= :CKs, Status = 1 WHERE sessionId = :sessionId;", use(m_Ns), use(m_PN), use(DHrStatusInt), use(DHs), use(RK), use(CKs), use(m_dbSessionId);
					}
						break;
					case DRSessionDbStatus::dirty_decrypt: // decrypt modifies: CKr, Nr and DHrStatus. Also set Status to active and clear X3DH init message if there is one(it is actually useless as our first reply from peer shall trigger a ratchet&decrypt)
					{
						// make sure we have no other session active with this pair local,peer DiD
						if (m_active_status == false) {
							m_localStorage->sql<<"UPDATE DR_sessions SET Status = 0, timeStamp = CURRENT_TIMESTAMP WHERE Did = :Did AND Uid = :Uid", use(m_peerDid), use(m_db_Uid);
							m_active_status = true;
						}
						int DHrStatusInt = DHrStatusToInt();

						blob CKr(m_localStorage->sql);
						CKr.write(0, (char *)(m_CKr.data()), m_CKr.size());
						m_localStorage->sql<<"UPDATE DR_sessions SET Nr= :Nr, CKr= :CKr, DHrStatus= :DHrStatus, Status = 1, X3DHInit = NULL WHERE sessionId = :sessionId;", use(m_Nr), use(CKr), use(DHrStatusInt), use(m_dbSessionId);
					}
						break;
					case DRSessionDbStatus::dirty_encrypt: // encrypt modifies: CKs and Ns
					{
						blob CKs(m_localStorage->sql);
						CKs.write(0, (char *)(m_CKs.data()), m_CKs.size());
						int status = (m_active_status==true)?0x01:0x00;
						m_localStorage->sql<<"UPDATE DR_sessions SET Ns= :Ns, CKs= :CKs, Status = :active_status WHERE sessionId = :sessionId;", use(m_Ns), use(CKs), use(status), use(m_dbSessionId);
					}
						break;
					case DRSessionDbStatus::clean: // Session is clean? So why have we been called?
					default:
						LIME_LOGE<<"Double ratchet session saved call on sessionId "<<m_dbSessionId<<" but sessions appears to be clean";
						break;
				}

				// updatesert went well, do we have any mkskipped row to modify
				if (m_usedDHid !=0 ) { // ok, we consumed a key, remove it from db
					m_localStorage->sql<<"DELETE from DR_MSk_MK WHERE DHid = :DHid AND Nr = :Nr;", use(m_usedDHid), use(m_usedNr);
					MSk_DHr_Clean = true; // flag the cleaning needed in DR_MSk_DH table, we may have to remove a row in it if no more row are linked to it in DR_MSk_MK
				} else { // we did not consume a key
					if (m_dirty == DRSessionDbStatus::dirty_decrypt || m_dirty == DRSessionDbStatus::dirty_ratchet_receiving) { // if we did a message decrypt :
						// update the count of posterior messages received in the stored skipped messages keys for this session (all stored chains)
						m_localStorage->sql<<"UPDATE DR_MSk_DHr SET received = received + 1 WHERE sessionId = :sessionId", use(m_dbSessionId);
					}
				}
			}

			// Shall we insert some skipped Message keys?
			for ( const auto &rChain : m_mkskipped) { // loop all chains of message keys, each one is a DHr associated to an unordered map of MK indexed by Nr to be saved
				blob DHr(m_localStorage->sql);
				DHr.write(0, (char *)(rChain.DHrIndex.data()), rChain.DHrIndex.size());
				long DHid=0;
				m_localStorage->sql<<"SELECT DHid FROM DR_MSk_DHr WHERE sessionId = :sessionId AND DHr = :DHr LIMIT 1;",into(DHid), use(m_dbSessionId), use(DHr);
				if (!m_localStorage->sql.got_data()) { // There is no row in DR_MSk_DHr matching this key, we must add it
					m_localStorage->sql<<"INSERT INTO DR_MSk_DHr(sessionId, DHr) VALUES(:sessionId, :DHr)", use(m_dbSessionId), use(DHr);
					m_localStorage->sql<<"select last_insert_rowid()",into(DHid); // WARNING: unportable code, sqlite3 only, see above for more details on similar issue
				} else { // the chain already exists in storage, just reset its counter of newer message received
					m_localStorage->sql<<"UPDATE DR_MSk_DHr SET received = 0 WHERE DHid = :DHid", use(DHid);
				}
				// insert all the skipped key in the chain
				uint16_t Nr;
				blob MK(m_localStorage->sql);
				statement st = (m_localStorage->sql.prepare << "INSERT INTO DR_MSk_MK(DHid,Nr,MK) VALUES(:DHid,:Nr,:Mk)", use(DHid), use(Nr), use(MK));

				for (const auto &kv : rChain.messageKeys) { // messageKeys is an unordered map of MK indexed by Nr.
					Nr=kv.first;
					MK.write(0, (char *)kv.second.data(), kv.second.size());
					st.execute(true);
				}
			}

			// Now do the cleaning (remove unused row from DR_MKs_DHr table) if needed
			if (MSk_DHr_Clean == true) {
				uint16_t Nr;
				m_localStorage->sql<<"SELECT Nr from DR_MSk_MK WHERE DHid = :DHid LIMIT 1;", into(Nr), use(m_usedDHid);
				if (!m_localStorage->sql.got_data()) { // no more MK with this DHid, remove it
					m_localStorage->sql<<"DELETE from DR_MSk_DHr WHERE DHid = :DHid;", use(m_usedDHid);
				}
			}
		} catch (exception const &e) {
			if (commit) {
				m_localStorage->rollback_transaction();
			}
			throw BCTBX_EXCEPTION << "Lime save session in DB failed. DB backend says : "<<e.what();
		}

		if (commit) {
			m_localStorage->commit_transaction();
		}
		return true;
	};

	/**
	 * @brief Load a session from the local storage based on m_dbSessionId
	 *
	 */
	template <typename Curve>
	bool DRi<Curve>::session_load() {
		std::lock_guard<std::recursive_mutex> lock(*(m_localStorage->m_db_mutex));

		// blobs to store DR session data
		blob DHr(m_localStorage->sql);
		blob DHs(m_localStorage->sql);
		blob RK(m_localStorage->sql);
		blob CKs(m_localStorage->sql);
		blob CKr(m_localStorage->sql);
		blob AD(m_localStorage->sql);
		blob X3DH_initMessage(m_localStorage->sql);

		// create an empty DR session
		indicator ind;
		int status; // retrieve an int from DB, turn it into a bool to store in object
		int DHrStatus; // retrieve an int from DB, turn it into  bools to store in object
		m_localStorage->sql<<"SELECT Did,Uid,Ns,Nr,PN,DHr,DHrStatus,DHs,RK,CKs,CKr,AD,Status,X3DHInit,strftime('%s',timeStamp) FROM DR_sessions WHERE sessionId = :sessionId LIMIT 1", into(m_peerDid), into(m_db_Uid), into(m_Ns), into(m_Nr), into(m_PN), into(DHr), into(DHrStatus), into(DHs), into(RK), into(CKs), into(CKr), into(AD), into(status), into(X3DH_initMessage,ind), into(m_lastKEMRatchetEpoch), use(m_dbSessionId);

		if (m_localStorage->sql.got_data()) { // TODO : some more specific checks on length of retrieved data?
			typename ARrKey<Curve>::serializedBuffer serializedDHr{};
			DHr.read(0, (char *)(serializedDHr.data()), ARrKey<Curve>::serializedSize());
			m_ARKeys.setDHr(serializedDHr);
			typename ARsKey<Curve>::serializedBuffer serializedDHs{};
			DHs.read(0, (char *)(serializedDHs.data()), ARsKey<Curve>::serializedSize());
			m_ARKeys.setDHs(serializedDHs);
			RK.read(0, (char *)(m_RK.data()), m_RK.size());
			CKs.read(0, (char *)(m_CKs.data()), m_CKs.size());
			CKr.read(0, (char *)(m_CKr.data()), m_CKr.size());
			AD.read(0, (char *)(m_sharedAD.data()), m_sharedAD.size());
			if (ind == i_ok && X3DH_initMessage.get_len()>0) {
				m_X3DH_initMessage.resize(X3DH_initMessage.get_len());
				X3DH_initMessage.read(0, (char *)(m_X3DH_initMessage.data()), m_X3DH_initMessage.size());
			}
			if (status==1) {
				m_active_status = true;
			} else {
				m_active_status = false;
			}
			// set session information from the stored DHrStatus
			IntToDHrStatus(DHrStatus);
			return true;
		} else { // something went wrong with the DB, we cannot retrieve the session
			return false;
		}
	};

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
	void DRi<Curve>::skipMessageKeys(const uint16_t until, const int limit) {
		if (m_Nr==until) return; // just to be sure we actually have MK to derive and store

		// check if there are not too much message keys to derive in this chain
		if (m_Nr + limit < until) {
			throw BCTBX_EXCEPTION << "DR Session is too far behind this message to derive requested amount of keys: "<<(until-m_Nr);
		}

		// each call to this function is made with a different DHrIndex
		ReceiverKeyChain<Curve> newRChain{m_ARKeys.getDHr().getIndex()};
		m_mkskipped.push_back(newRChain);
		auto rChain = &m_mkskipped.back();

		DRMKey MK;
		while (m_Nr<until) {
			KDF_CK<Curve>(m_CKr, MK, m_Nr);
			// insert the nessage key into the list of skipped ones
			rChain->messageKeys[m_Nr]=MK;
			m_Nr++;
			m_KEMRatchetChainSize++;
		}
	}

	/**
	 * @brief retrieve stored message key indexed by given DHr and Nr
	 *
	 * @param[in]	DHrIndex	a key computed from peer DHr to index the MK chain
	 * @param[in]	Nr		Nr index in the DHr indexed MK chain
	 * @param[out]	MK		the message key retrieved from db
	 *
	 * @return	true if the message key was found in db
	 */
	template <typename Curve>
	bool DRi<Curve>::trySkippedMessageKeys(const uint16_t Nr, const std::vector<uint8_t> &DHrIndex, DRMKey &MK) {
		std::lock_guard<std::recursive_mutex> lock(*(m_localStorage->m_db_mutex));
		blob MK_blob(m_localStorage->sql);
		blob DHr_blob(m_localStorage->sql);
		DHr_blob.write(0, (char *)(DHrIndex.data()), DHrIndex.size());

		indicator ind;
		m_localStorage->sql<<"SELECT m.MK, m.DHid FROM DR_MSk_MK as m INNER JOIN DR_MSk_DHr as d ON d.DHid=m.DHid WHERE d.sessionId = :sessionId AND d.DHr = :DHr AND m.Nr = :Nr LIMIT 1", into(MK_blob,ind), into(m_usedDHid), use(m_dbSessionId), use(DHr_blob), use(Nr);
		// we didn't find anything
		if (!m_localStorage->sql.got_data() || ind != i_ok || MK_blob.get_len()!=MK.size()) {
			m_usedDHid=0; // make sure the DHid is not set when we didn't find anything as it is later used to remove confirmed used key from DB
			return false;
		}
		// record the Nr of extracted to be able to delete it fron base later (if decrypt ends well)
		m_usedNr=Nr;

		MK_blob.read(0, (char *)(MK.data()), MK.size());
		return true;
	};



	/****************************************************************************/
	/* factory functions                                                        */
	/****************************************************************************/

	template <typename Algo> std::shared_ptr<DR> make_DR_from_localStorage(std::shared_ptr<lime::Db> localStorage, long sessionId, std::shared_ptr<RNG> RNG_context) {
		return std::static_pointer_cast<DR>(std::make_shared<DRi<Algo>>(localStorage, sessionId, RNG_context));
	}
	template <typename Algo> std::shared_ptr<DR> make_DR_for_sender(std::shared_ptr<lime::Db> localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const ARrKey<Algo> &peerPublicKey, long int peerDid, const std::string &peerDeviceId, const DSA<typename Algo::EC, lime::DSAtype::publicKey> &peerIk, long int selfDid, const std::vector<uint8_t> &X3DH_initMessage, std::shared_ptr<RNG> RNG_context) {
		return std::static_pointer_cast<DR>(std::make_shared<DRi<Algo>>(localStorage, SK, AD, peerPublicKey, peerDid, peerDeviceId, peerIk, selfDid, X3DH_initMessage, RNG_context));
	}
	template <typename Algo> std::shared_ptr<DR> make_DR_for_receiver(std::shared_ptr<lime::Db> localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const ARsKey<Algo> &selfKeyPair, long int peerDid, const std::string &peerDeviceId, const uint32_t OPk_id, const DSA<typename Algo::EC, lime::DSAtype::publicKey> &peerIk, long int selfDeviceId, std::shared_ptr<RNG> RNG_context) {
		return std::static_pointer_cast<DR>(std::make_shared<DRi<Algo>>(localStorage, SK, AD, selfKeyPair, peerDid, peerDeviceId, OPk_id, peerIk, selfDeviceId, RNG_context));
	}


/* template instanciations */
#ifdef EC25519_ENABLED
	template class DRi<C255>;

	template std::shared_ptr<DR> make_DR_from_localStorage<C255>(std::shared_ptr<lime::Db> localStorage, long sessionId, std::shared_ptr<RNG> RNG_context);
	template std::shared_ptr<DR> make_DR_for_sender(std::shared_ptr<lime::Db> localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const ARrKey<C255> &peerPublicKey, long int peerDid, const std::string &peerDeviceId, const DSA<C255::EC, lime::DSAtype::publicKey> &peerIk, long int selfDid, const std::vector<uint8_t> &X3DH_initMessage, std::shared_ptr<RNG> RNG_context);
	template std::shared_ptr<DR> make_DR_for_receiver(std::shared_ptr<lime::Db> localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const ARsKey<C255> &selfKeyPair, long int peerDid, const std::string &peerDeviceId, const uint32_t OPk_id, const DSA<C255::EC, lime::DSAtype::publicKey> &peerIk, long int selfDeviceId, std::shared_ptr<RNG> RNG_context);
#endif

#ifdef EC448_ENABLED
	template class DRi<C448>;

	template std::shared_ptr<DR> make_DR_from_localStorage<C448>(std::shared_ptr<lime::Db> localStorage, long sessionId, std::shared_ptr<RNG> RNG_context);
	template std::shared_ptr<DR> make_DR_for_sender(std::shared_ptr<lime::Db> localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const ARrKey<C448> &peerPublicKey, long int peerDid, const std::string &peerDeviceId, const DSA<C448::EC, lime::DSAtype::publicKey> &peerIk, long int selfDid, const std::vector<uint8_t> &X3DH_initMessage, std::shared_ptr<RNG> RNG_context);
	template std::shared_ptr<DR> make_DR_for_receiver(std::shared_ptr<lime::Db> localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const ARsKey<C448> &selfKeyPair, long int peerDid, const std::string &peerDeviceId, const uint32_t OPk_id, const DSA<C448::EC, lime::DSAtype::publicKey> &peerIk, long int selfDeviceId, std::shared_ptr<RNG> RNG_context);
#endif
#ifdef HAVE_BCTBXPQ
	template class DRi<C255K512>;
	template std::shared_ptr<DR> make_DR_from_localStorage<C255K512>(std::shared_ptr<lime::Db> localStorage, long sessionId, std::shared_ptr<RNG> RNG_context);
	template std::shared_ptr<DR> make_DR_for_sender(std::shared_ptr<lime::Db> localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const ARrKey<C255K512> &peerPublicKey, long int peerDid, const std::string &peerDeviceId, const DSA<C255K512::EC, lime::DSAtype::publicKey> &peerIk, long int selfDid, const std::vector<uint8_t> &X3DH_initMessage, std::shared_ptr<RNG> RNG_context);
	template std::shared_ptr<DR> make_DR_for_receiver(std::shared_ptr<lime::Db> localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const ARsKey<C255K512> &selfKeyPair, long int peerDid, const std::string &peerDeviceId, const uint32_t OPk_id, const DSA<C255K512::EC, lime::DSAtype::publicKey> &peerIk, long int selfDeviceId, std::shared_ptr<RNG> RNG_context);
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
	void encryptMessage(std::vector<RecipientInfos>& recipients, const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& recipientUserId, const std::string& sourceDeviceId, std::vector<uint8_t>& cipherMessage, const lime::EncryptionPolicy encryptionPolicy, std::shared_ptr<lime::Db> localStorage) {
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
		std::vector<uint8_t> randomSeed(lime::settings::DRrandomSeedSize); // this seed is sent in DR message and used to derivate random key + IV to encrypt the actual message

		if (!payloadDirectEncryption) { // Payload is encrypted in a separate cipher message buffer while the key used to encrypt it is in the DR message
			// First generate a key and IV, use it to encrypt the given message, Associated Data are : sourceDeviceId || recipientUserId
			// generate the random seed
			auto RNG_context = make_RNG();
			RNG_context->randomize(randomSeed.data(), lime::settings::DRrandomSeedSize);

			// expansion of randomSeed to 48 bytes: 32 bytes random key + 16 bytes nonce, use HKDF with empty salt
			std::vector<uint8_t> emptySalt{};
			lime::sBuffer<lime::settings::DRMessageKeySize+lime::settings::DRMessageIVSize> randomKey;
			HMAC_KDF<SHA512>(emptySalt.data(), emptySalt.size(), randomSeed.data(), randomSeed.size(), lime::settings::hkdf_randomSeed_info.data(), lime::settings::hkdf_randomSeed_info.size(), randomKey.data(), randomKey.size());

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
			if (!payloadDirectEncryption) {
				cleanBuffer(randomSeed.data(), lime::settings::DRrandomSeedSize);
			}
		} catch (BctbxException const &e) {
			localStorage->rollback_transaction();
			throw BCTBX_EXCEPTION << "Encryption to recipients failed : "<<e.str();
		} catch (exception const &e) {
			localStorage->rollback_transaction();
			throw BCTBX_EXCEPTION << "Encryption to recipients failed : "<<e.what();
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
	std::shared_ptr<DR> decryptMessage(const std::string& sourceDeviceId, const std::string& recipientDeviceId, const std::vector<uint8_t>& recipientUserId, std::vector<std::shared_ptr<DR>>& DRSessions, const std::vector<uint8_t>& DRmessage, const std::vector<uint8_t>& cipherMessage, std::vector<uint8_t>& plaintext) {
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
		std::vector<uint8_t> randomSeed(lime::settings::DRrandomSeedSize);

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
				HMAC_KDF<SHA512>(emptySalt.data(), emptySalt.size(), randomSeed.data(), randomSeed.size(), lime::settings::hkdf_randomSeed_info.data(), lime::settings::hkdf_randomSeed_info.size(), randomKey.data(), randomKey.size());
				cleanBuffer(randomSeed.data(), lime::settings::DRrandomSeedSize);

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
}
