/*
 * Copyright (c) 2020 Belledonne Communications SARL.
 *
 * This file is part of postquantumcryptoengine.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef POSTQUANTUMCRYPTO_HH
#define POSTQUANTUMCRYPTO_HH

#include <vector>
#include <memory>
#include <string>
#include <list>
#include "bctoolbox/crypto.hh"
#include "bctoolbox/exception.hh"
#include "postquantumcryptoengine/crypto.h"

namespace bctoolbox {

class HYBRID_KEM;
/************************ KEM interface ************************/
/**
 * @brief The KEM vitual class
 *		Declares all functions that KEM algorithms need
 */
class KEM {
public:
	virtual ~KEM() = default;
	virtual int crypto_kem_keypair(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept = 0;
	virtual int crypto_kem_enc(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept = 0;
	virtual int crypto_kem_dec(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept = 0;
private:
	virtual size_t get_skSize() const = 0;
	virtual size_t get_pkSize() const = 0;
	virtual size_t get_ctSize() const = 0;
	virtual size_t get_ssSize() const = 0;
friend class HYBRID_KEM;
};

/**
 * @brief The KEM CRTP(Curiously Recurring Template Pattern) class
 * implement getters functions
 */
template <typename Derived> class KEMCRTP : public KEM {
public:
	virtual int crypto_kem_keypair(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override = 0;
	virtual int crypto_kem_enc(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override = 0;
	virtual int crypto_kem_dec(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override = 0;
private:
	size_t get_skSize() const override;
	size_t get_pkSize() const override;
	size_t get_ctSize() const override;
	size_t get_ssSize() const override;
};

/**
 * @brief The ECDH_KEM class extends the KEM class
 *		Declares all attributs that ECDH KEM algorithms need
 *		Implements all functions that ECDH KEM algorithms need
 *		Pass the Derived template argument to KEMCRTP so getters implementation
 *		will directly access the derived class
 */
template <typename Derived> class ECDH_KEM : public KEMCRTP<Derived> {
protected:
	uint8_t mId;		/**< Id of the key agreement algorithm defined in the RFC https://datatracker.ietf.org/doc/html/rfc9180#section-7.1 */
	int mName;		/**< Name of the key agreement algorithm */
	int mHashId;	/**< Id of the hash algorithm */

public:
	int crypto_kem_keypair(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	/* INFO : enc and dec return the derivation of shared secret | REF : https://datatracker.ietf.org/doc/html/draft-irtf-cfrg-hpke-12 */
	int crypto_kem_enc(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int crypto_kem_dec(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The K25519 class extends the ECDH_KEM class
 *		Initialises all key size parameters
 */
class K25519 : public ECDH_KEM<K25519> {
public:
	K25519(int hash_id);	/**< hash_id param represents the id of the hash algorithm used in the secret derivation */
	constexpr static size_t skSize = BCTBX_ECDH_X25519_PRIVATE_SIZE;
	constexpr static size_t pkSize = BCTBX_ECDH_X25519_PUBLIC_SIZE;
	constexpr static size_t ctSize = BCTBX_ECDH_X25519_PUBLIC_SIZE;
	constexpr static size_t ssSize = BCTBX_ECDH_X25519_PUBLIC_SIZE;
};

/**
 * @brief The K448 class extends the ECDH_KEM class
 *		Initialises all key size parameters
 */
class K448 : public ECDH_KEM<K448> {
public:
	K448(int hash_id);		/**< hash_id param represents the id of the hash algorithm used in the secret derivation */
	constexpr static size_t skSize = BCTBX_ECDH_X448_PRIVATE_SIZE;
	constexpr static size_t pkSize = BCTBX_ECDH_X448_PUBLIC_SIZE;
	constexpr static size_t ctSize = BCTBX_ECDH_X448_PUBLIC_SIZE;
	constexpr static size_t ssSize = BCTBX_ECDH_X448_PUBLIC_SIZE;
};

/**
 * @brief The MLKEM512 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that MLKEM512 algorithm needs
 */
class MLKEM512 : public KEMCRTP<MLKEM512> {
public:
	constexpr static size_t skSize = 1632;
	constexpr static size_t pkSize = 800;
	constexpr static size_t ctSize = 768;
	constexpr static size_t ssSize = 32;

	int crypto_kem_keypair(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int crypto_kem_enc(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int crypto_kem_dec(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The MLKEM768 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that MLKEM768 algorithm needs
 */
class MLKEM768 : public KEMCRTP<MLKEM768> {
public:
	constexpr static size_t skSize = 2400;
	constexpr static size_t pkSize = 1184;
	constexpr static size_t ctSize = 1088;
	constexpr static size_t ssSize = 32;

	int crypto_kem_keypair(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int crypto_kem_enc(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int crypto_kem_dec(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The MLKEM1024 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that MLKEM1024 algorithm needs
 */
class MLKEM1024 : public KEMCRTP<MLKEM1024> {
public:
	constexpr static size_t skSize = 3168;
	constexpr static size_t pkSize = 1568;
	constexpr static size_t ctSize = 1568;
	constexpr static size_t ssSize = 32;

	int crypto_kem_keypair(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int crypto_kem_enc(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int crypto_kem_dec(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The KYBER512 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that KYBER512 algorithm needs
 */
class KYBER512 : public KEMCRTP<KYBER512> {
public:
	constexpr static size_t skSize = 1632;
	constexpr static size_t pkSize = 800;
	constexpr static size_t ctSize = 768;
	constexpr static size_t ssSize = 32;

	int crypto_kem_keypair(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int crypto_kem_enc(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int crypto_kem_dec(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The KYBER768 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that KYBER768 algorithm needs
 */
class KYBER768 : public KEMCRTP<KYBER768> {
public:
	constexpr static size_t skSize = 2400;
	constexpr static size_t pkSize = 1184;
	constexpr static size_t ctSize = 1088;
	constexpr static size_t ssSize = 32;

	int crypto_kem_keypair(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int crypto_kem_enc(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int crypto_kem_dec(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The KYBER1024 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that KYBER1024 algorithm needs
 */
class KYBER1024 : public KEMCRTP<KYBER1024> {
public:
	constexpr static size_t skSize = 3168;
	constexpr static size_t pkSize = 1568;
	constexpr static size_t ctSize = 1568;
	constexpr static size_t ssSize = 32;

	int crypto_kem_keypair(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int crypto_kem_enc(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int crypto_kem_dec(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The HQC128 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that HQC128 algorithm needs
 */
class HQC128 : public KEMCRTP<HQC128> {
public:
	constexpr static size_t skSize = 2305;
	constexpr static size_t pkSize = 2249;
	constexpr static size_t ctSize = 4433;
	constexpr static size_t ssSize = 64;

	int crypto_kem_keypair(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int crypto_kem_enc(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int crypto_kem_dec(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The HQC192 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that HQC192 algorithm needs
 */
class HQC192 : public KEMCRTP<HQC192> {
public:
	constexpr static size_t skSize = 4586;
	constexpr static size_t pkSize = 4522;
	constexpr static size_t ctSize = 8978;
	constexpr static size_t ssSize = 64;

	int crypto_kem_keypair(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int crypto_kem_enc(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int crypto_kem_dec(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The HQC256 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that HQC256 algorithm needs
 */
class HQC256 : public KEMCRTP<HQC256> {
public:
	constexpr static size_t skSize = 7317;
	constexpr static size_t pkSize = 7245;
	constexpr static size_t ctSize = 14421;
	constexpr static size_t ssSize = 64;

	int crypto_kem_keypair(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int crypto_kem_enc(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int crypto_kem_dec(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The HYBRID_KEM class extends the KEM class
 *		Represents a hybrid KEM
 *		The KEM functions encapsulate, decapsulate several keys from several key exchange algorithms and combine them using the N-combiner
 */
class HYBRID_KEM : public KEMCRTP<HYBRID_KEM> {
private:
	std::list<std::shared_ptr<KEM>> mAlgo;	/**< List of the algorithms used in the hybrid KEM */
	int mHashId;							/**< Id of the hash algorithm */

public:
	// Dummy values, the getters function are never called on an hybrid kem object, it will trigger an exception if it is.
	constexpr static size_t skSize = 0;
	constexpr static size_t pkSize = 0;
	constexpr static size_t ctSize = 0;
	constexpr static size_t ssSize = 0;

	HYBRID_KEM(const std::list<std::shared_ptr<KEM>> &algoList, int hash_id);

	int crypto_kem_keypair(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int crypto_kem_enc(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int crypto_kem_dec(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

// already instanciated templates
extern template class ECDH_KEM<K25519>;
extern template class ECDH_KEM<K448>;

} // namespace bctoolbox

#endif // POSTQUANTUMCRYPTO_HH

