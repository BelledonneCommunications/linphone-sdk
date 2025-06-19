/*
 * Copyright (c) 2020-2025 Belledonne Communications SARL.
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

// Helper struct to detect the presence of kSkSize member
// When this member is present, the other size are defined too
// when it is not, use the template type defined getter
template<typename, typename = std::void_t<>>
struct hasSize : std::false_type {};

template<typename T>
struct hasSize<T, std::void_t<decltype(T::kSkSize)>> : std::true_type {};

/************************ KEM interface ************************/
/**
 * @brief The KEM vitual class
 *		Declares all functions that KEM algorithms need
 */
class KEM {
public:
	virtual ~KEM() = default;
	virtual size_t getSkSize() const noexcept = 0;
	virtual size_t getPkSize() const noexcept = 0;
	virtual size_t getCtSize() const noexcept = 0;
	virtual size_t getSsSize() const noexcept = 0;
	virtual int keyGen(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept = 0;
	virtual int encaps(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept = 0;
	virtual int decaps(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept = 0;
private:
};

/**
 * @brief The KEM CRTP(Curiously Recurring Template Pattern) class
 * implement getters functions
 */
template <typename Derived> class KEMCRTP : public KEM {
public:
	size_t getSkSize() const noexcept override { return getSkSizeImpl(hasSize<Derived>{});};
	size_t getPkSize() const noexcept override { return getPkSizeImpl(hasSize<Derived>{});};
	size_t getCtSize() const noexcept override { return getCtSizeImpl(hasSize<Derived>{});};
	size_t getSsSize() const noexcept override { return getSsSizeImpl(hasSize<Derived>{});};
	virtual int keyGen(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override = 0;
	virtual int encaps(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override = 0;
	virtual int decaps(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override = 0;
private:
	/* tag dispatching :
	 *  - when the Derived type defines the sizes of element directly, return them - CRTP
	 *  - when it does not, return their own defined getters */
	size_t getSkSizeImpl(std::true_type) const noexcept { return Derived::kSkSize;};
	size_t getSkSizeImpl(std::false_type) const noexcept { return static_cast<const Derived*>(this)->getSkSize();};
	size_t getPkSizeImpl(std::true_type) const noexcept { return Derived::kPkSize;};
	size_t getPkSizeImpl(std::false_type) const noexcept { return static_cast<const Derived*>(this)->getPkSize();};
	size_t getCtSizeImpl(std::true_type) const noexcept { return Derived::kCtSize;};
	size_t getCtSizeImpl(std::false_type) const noexcept { return static_cast<const Derived*>(this)->getCtSize();};
	size_t getSsSizeImpl(std::true_type) const noexcept { return Derived::kSsSize;};
	size_t getSsSizeImpl(std::false_type) const noexcept { return static_cast<const Derived*>(this)->getSsSize();};
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
	int keyGen(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	/* INFO : enc and dec return the derivation of shared secret | REF : https://datatracker.ietf.org/doc/html/draft-irtf-cfrg-hpke-12 */
	int encaps(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int decaps(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The K25519 class extends the ECDH_KEM class
 *		Initialises all key size parameters
 */
class K25519 : public ECDH_KEM<K25519> {
public:
	K25519(int hashId);	/**< hashId param represents the id of the hash algorithm used in the secret derivation */
	constexpr static size_t kSkSize = BCTBX_ECDH_X25519_PRIVATE_SIZE;
	constexpr static size_t kPkSize = BCTBX_ECDH_X25519_PUBLIC_SIZE;
	constexpr static size_t kCtSize = BCTBX_ECDH_X25519_PUBLIC_SIZE;
	constexpr static size_t kSsSize = BCTBX_ECDH_X25519_PUBLIC_SIZE;
};

/**
 * @brief The K448 class extends the ECDH_KEM class
 *		Initialises all key size parameters
 */
class K448 : public ECDH_KEM<K448> {
public:
	K448(int hashId);		/**< hashId param represents the id of the hash algorithm used in the secret derivation */
	constexpr static size_t kSkSize = BCTBX_ECDH_X448_PRIVATE_SIZE;
	constexpr static size_t kPkSize = BCTBX_ECDH_X448_PUBLIC_SIZE;
	constexpr static size_t kCtSize = BCTBX_ECDH_X448_PUBLIC_SIZE;
	constexpr static size_t kSsSize = BCTBX_ECDH_X448_PUBLIC_SIZE;
};

/**
 * @brief The MLKEM512 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that MLKEM512 algorithm needs
 */
class MLKEM512 : public KEMCRTP<MLKEM512> {
public:
	constexpr static size_t kSkSize = 1632;
	constexpr static size_t kPkSize = 800;
	constexpr static size_t kCtSize = 768;
	constexpr static size_t kSsSize = 32;

	int keyGen(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int encaps(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int decaps(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The MLKEM768 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that MLKEM768 algorithm needs
 */
class MLKEM768 : public KEMCRTP<MLKEM768> {
public:
	constexpr static size_t kSkSize = 2400;
	constexpr static size_t kPkSize = 1184;
	constexpr static size_t kCtSize = 1088;
	constexpr static size_t kSsSize = 32;

	int keyGen(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int encaps(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int decaps(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The MLKEM1024 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that MLKEM1024 algorithm needs
 */
class MLKEM1024 : public KEMCRTP<MLKEM1024> {
public:
	constexpr static size_t kSkSize = 3168;
	constexpr static size_t kPkSize = 1568;
	constexpr static size_t kCtSize = 1568;
	constexpr static size_t kSsSize = 32;

	int keyGen(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int encaps(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int decaps(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The KYBER512 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that KYBER512 algorithm needs
 */
class KYBER512 : public KEMCRTP<KYBER512> {
public:
	constexpr static size_t kSkSize = 1632;
	constexpr static size_t kPkSize = 800;
	constexpr static size_t kCtSize = 768;
	constexpr static size_t kSsSize = 32;

	int keyGen(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int encaps(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int decaps(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The KYBER768 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that KYBER768 algorithm needs
 */
class KYBER768 : public KEMCRTP<KYBER768> {
public:
	constexpr static size_t kSkSize = 2400;
	constexpr static size_t kPkSize = 1184;
	constexpr static size_t kCtSize = 1088;
	constexpr static size_t kSsSize = 32;

	int keyGen(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int encaps(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int decaps(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The KYBER1024 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that KYBER1024 algorithm needs
 */
class KYBER1024 : public KEMCRTP<KYBER1024> {
public:
	constexpr static size_t kSkSize = 3168;
	constexpr static size_t kPkSize = 1568;
	constexpr static size_t kCtSize = 1568;
	constexpr static size_t kSsSize = 32;

	int keyGen(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int encaps(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int decaps(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The HQC128 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that HQC128 algorithm needs
 */
class HQC128 : public KEMCRTP<HQC128> {
public:
	constexpr static size_t kSkSize = 2305;
	constexpr static size_t kPkSize = 2249;
	constexpr static size_t kCtSize = 4433;
	constexpr static size_t kSsSize = 64;

	int keyGen(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int encaps(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int decaps(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The HQC192 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that HQC192 algorithm needs
 */
class HQC192 : public KEMCRTP<HQC192> {
public:
	constexpr static size_t kSkSize = 4586;
	constexpr static size_t kPkSize = 4522;
	constexpr static size_t kCtSize = 8978;
	constexpr static size_t kSsSize = 64;

	int keyGen(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int encaps(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int decaps(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

/**
 * @brief The HQC256 class extends the KEM class
 *		Initialises all key size parameters
 *		Implements all functions that HQC256 algorithm needs
 */
class HQC256 : public KEMCRTP<HQC256> {
public:
	constexpr static size_t kSkSize = 7317;
	constexpr static size_t kPkSize = 7245;
	constexpr static size_t kCtSize = 14421;
	constexpr static size_t kSsSize = 64;

	int keyGen(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int encaps(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int decaps(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
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
	HYBRID_KEM(const std::list<std::shared_ptr<KEM>> &algoList, int hashId);
	size_t getSkSize() const noexcept override;
	size_t getPkSize() const noexcept override;
	size_t getCtSize() const noexcept override;
	size_t getSsSize() const noexcept override;

	int keyGen(std::vector<uint8_t> &pk, std::vector<uint8_t> &sk) const noexcept override;
	int encaps(std::vector<uint8_t> &ct, std::vector<uint8_t> &ss, const std::vector<uint8_t> &pk) const noexcept override;
	int decaps(std::vector<uint8_t> &ss, const std::vector<uint8_t> &ct, const std::vector<uint8_t> &sk) const noexcept override;
};

// already instanciated templates
extern template class ECDH_KEM<K25519>;
extern template class ECDH_KEM<K448>;

} // namespace bctoolbox

#endif // POSTQUANTUMCRYPTO_HH

