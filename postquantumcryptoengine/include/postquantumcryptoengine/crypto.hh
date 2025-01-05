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
#include "postquantumcryptoengine/crypto.h"

namespace bctoolbox {

/************************ KEM interface ************************/
/**
 * @brief The KEM vitual class
 *		Declares all functions that KEM algorithms need
 */
class KEM {
public:
	virtual ~KEM() = default;
	virtual size_t get_skSize() const noexcept = 0;
	virtual size_t get_pkSize() const noexcept = 0;
	virtual size_t get_ctSize() const noexcept = 0;
	virtual size_t get_ssSize() const noexcept = 0;
	virtual int crypto_kem_keypair(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept = 0;
	virtual int crypto_kem_enc(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept = 0;
	virtual int crypto_kem_dec(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept = 0;
};

/**
 * @brief The ECDH_KEM class extends the KEM class
 *		Declares all attributs that ECDH KEM algorithms need
 *		Implements all functions that ECDH KEM algorithms need
 */
class ECDH_KEM : public KEM {
protected:
	uint8_t id;		/**< Id of the key agreement algorithm defined in the RFC https://datatracker.ietf.org/doc/html/rfc9180#section-7.1 */
	int name;		/**< Name of the key agreement algorithm */
	int hash_id;	/**< Id of the hash algorithm */

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
class K25519 : public ECDH_KEM {
public:
	K25519(int hash_id);	/**< hash_id param represents the id of the hash algorithm used in the secret derivation */
	constexpr static size_t skSize = BCTBX_ECDH_X25519_PRIVATE_SIZE;
	constexpr static size_t pkSize = BCTBX_ECDH_X25519_PUBLIC_SIZE;
	constexpr static size_t ctSize = BCTBX_ECDH_X25519_PUBLIC_SIZE;
	constexpr static size_t ssSize = BCTBX_ECDH_X25519_PUBLIC_SIZE;
	size_t get_skSize() const noexcept override;
	size_t get_pkSize() const noexcept override;
	size_t get_ctSize() const noexcept override;
	size_t get_ssSize() const noexcept override;
};

/**
 * @brief The K448 class extends the ECDH_KEM class
 *		Initialises all key size parameters
 */
class K448 : public ECDH_KEM {
public:
	K448(int hash_id);		/**< hash_id param represents the id of the hash algorithm used in the secret derivation */
	constexpr static size_t skSize = BCTBX_ECDH_X448_PRIVATE_SIZE;
	constexpr static size_t pkSize = BCTBX_ECDH_X448_PUBLIC_SIZE;
	constexpr static size_t ctSize = BCTBX_ECDH_X448_PUBLIC_SIZE;
	constexpr static size_t ssSize = BCTBX_ECDH_X448_PUBLIC_SIZE;
	size_t get_skSize() const noexcept override;
	size_t get_pkSize() const noexcept override;
	size_t get_ctSize() const noexcept override;
	size_t get_ssSize() const noexcept override;
};

/**
 * @brief The MLKEM512 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that MLKEM512 algorithm needs
 */
class MLKEM512 : public KEM {
public:
	MLKEM512() = default;
	constexpr static size_t skSize = 1632;
	constexpr static size_t pkSize = 800;
	constexpr static size_t ctSize = 768;
	constexpr static size_t ssSize = 32;

	size_t get_skSize() const noexcept override;
	size_t get_pkSize() const noexcept override;
	size_t get_ctSize() const noexcept override;
	size_t get_ssSize() const noexcept override;
	int crypto_kem_keypair(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int crypto_kem_enc(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int crypto_kem_dec(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The MLKEM768 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that MLKEM768 algorithm needs
 */
class MLKEM768 : public KEM {
public:
	MLKEM768() = default;
	constexpr static size_t skSize = 2400;
	constexpr static size_t pkSize = 1184;
	constexpr static size_t ctSize = 1088;
	constexpr static size_t ssSize = 32;

	size_t get_skSize() const noexcept override;
	size_t get_pkSize() const noexcept override;
	size_t get_ctSize() const noexcept override;
	size_t get_ssSize() const noexcept override;
	int crypto_kem_keypair(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int crypto_kem_enc(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int crypto_kem_dec(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The MLKEM1024 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that MLKEM1024 algorithm needs
 */
class MLKEM1024 : public KEM {
public:
	MLKEM1024() = default;
	constexpr static size_t skSize = 3168;
	constexpr static size_t pkSize = 1568;
	constexpr static size_t ctSize = 1568;
	constexpr static size_t ssSize = 32;

	size_t get_skSize() const noexcept override;
	size_t get_pkSize() const noexcept override;
	size_t get_ctSize() const noexcept override;
	size_t get_ssSize() const noexcept override;
	int crypto_kem_keypair(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int crypto_kem_enc(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int crypto_kem_dec(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The KYBER512 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that KYBER512 algorithm needs
 */
class KYBER512 : public KEM {
public:
	KYBER512() = default;
	constexpr static size_t skSize = 1632;
	constexpr static size_t pkSize = 800;
	constexpr static size_t ctSize = 768;
	constexpr static size_t ssSize = 32;

	size_t get_skSize() const noexcept override;
	size_t get_pkSize() const noexcept override;
	size_t get_ctSize() const noexcept override;
	size_t get_ssSize() const noexcept override;
	int crypto_kem_keypair(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int crypto_kem_enc(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int crypto_kem_dec(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The KYBER768 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that KYBER768 algorithm needs
 */
class KYBER768 : public KEM {
public:
	KYBER768() = default;
	constexpr static size_t skSize = 2400;
	constexpr static size_t pkSize = 1184;
	constexpr static size_t ctSize = 1088;
	constexpr static size_t ssSize = 32;

	size_t get_skSize() const noexcept override;
	size_t get_pkSize() const noexcept override;
	size_t get_ctSize() const noexcept override;
	size_t get_ssSize() const noexcept override;
	int crypto_kem_keypair(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int crypto_kem_enc(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int crypto_kem_dec(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The KYBER1024 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that KYBER1024 algorithm needs
 */
class KYBER1024 : public KEM {
public:
	KYBER1024() = default;
	constexpr static size_t skSize = 3168;
	constexpr static size_t pkSize = 1568;
	constexpr static size_t ctSize = 1568;
	constexpr static size_t ssSize = 32;

	size_t get_skSize() const noexcept override;
	size_t get_pkSize() const noexcept override;
	size_t get_ctSize() const noexcept override;
	size_t get_ssSize() const noexcept override;
	int crypto_kem_keypair(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int crypto_kem_enc(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int crypto_kem_dec(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The HQC128 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that HQC128 algorithm needs
 */
class HQC128 : public KEM {
public:
	HQC128() = default;
	constexpr static size_t skSize = 2305;
	constexpr static size_t pkSize = 2249;
	constexpr static size_t ctSize = 4433;
	constexpr static size_t ssSize = 64;

	size_t get_skSize() const noexcept override;
	size_t get_pkSize() const noexcept override;
	size_t get_ctSize() const noexcept override;
	size_t get_ssSize() const noexcept override;
	int crypto_kem_keypair(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int crypto_kem_enc(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int crypto_kem_dec(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The HQC192 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that HQC192 algorithm needs
 */
class HQC192 : public KEM {
public:
	HQC192() = default;
	constexpr static size_t skSize = 4586;
	constexpr static size_t pkSize = 4522;
	constexpr static size_t ctSize = 8978;
	constexpr static size_t ssSize = 64;

	size_t get_skSize() const noexcept override;
	size_t get_pkSize() const noexcept override;
	size_t get_ctSize() const noexcept override;
	size_t get_ssSize() const noexcept override;
	int crypto_kem_keypair(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int crypto_kem_enc(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int crypto_kem_dec(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The HQC256 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that HQC256 algorithm needs
 */
class HQC256 : public KEM {
public:
	HQC256() = default;
	constexpr static size_t skSize = 7317;
	constexpr static size_t pkSize = 7245;
	constexpr static size_t ctSize = 14421;
	constexpr static size_t ssSize = 64;

	size_t get_skSize() const noexcept override;
	size_t get_pkSize() const noexcept override;
	size_t get_ctSize() const noexcept override;
	size_t get_ssSize() const noexcept override;
	int crypto_kem_keypair(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int crypto_kem_enc(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int crypto_kem_dec(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The HYBRID_KEM class extends the KEM class
 *		Represents a hybrid KEM
 *		The KEM functions encapsulate, decapsulate several keys from several key exchange algorithms and combine them using the N-combiner
 */
class HYBRID_KEM : public KEM {
private:
	std::list<std::shared_ptr<KEM>> algo;	/**< List of the algorithms used in the hybrid KEM */
	int hash_id;							/**< Id of the hash algorithm */

public:
	HYBRID_KEM(const std::list<std::shared_ptr<KEM>> &, int);	/**< the int in param is the hash id */

	size_t get_skSize() const noexcept override;
	size_t get_pkSize() const noexcept override;
	size_t get_ctSize() const noexcept override;
	size_t get_ssSize() const noexcept override;
	int crypto_kem_keypair(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int crypto_kem_enc(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int crypto_kem_dec(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

} // namespace bctoolbox

#endif // POSTQUANTUMCRYPTO_HH

