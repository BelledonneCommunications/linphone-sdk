/*
	lime_defines.hpp
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

#ifndef lime_defines_hpp
#define lime_defines_hpp

#include <string>

namespace lime {
/** @brief Hold constants definition used as settings in all components of the lime library
 *
 * in lime_setting.hpp: you can tweak the behavior of the library.
 * No compatibility break between clients shall result by modifying this definitions
 * @note : you can tweak values but not the types, uint16_t values are intended to be bounded by 2^16 -1.
 *
 * in lime_defines.hpp: the constants defined cannot be modified without some work on the source code
 * unless you really know what you're doing, just leave them alone
 */
namespace settings {

/******************************************************************************/
/*                                                                            */
/* Double Ratchet related definitions                                         */
/*                                                                            */
/******************************************************************************/

	/// Sending, Receiving and Root key chain use 32 bytes keys (spec 3.2)
	constexpr size_t DRChainKeySize=32;
	/// String used as info in the root key derivation
	const std::string hkdf_DRChainKey_info{"DR Root Chain Key Derivation"};

	/// DR Message Key are composed of a 32 bytes key and 16 bytes of IV
	constexpr size_t DRMessageKeySize=32;
	/// DR Message Key are composed of a 32 bytes key and 16 bytes of IV
	constexpr size_t DRMessageIVSize=16;

	/** Size of the random seed used to generate the cipherMessage key
	 *
	 * Message Key is based on a message seed(sent in the DR message)
	 * Message key and nonce are derived(HKDF) from this seed and have the same length as DR Message Key
	 */
	constexpr size_t DRrandomSeedSize=32;
	/** info string used in the derivation(HKDF) of random seed into the key used to encrypt the cipherMessage key
	 *
	 * Message Key is based on a message seed(sent in the DR message)
	 * Message key and nonce are derived(HKDF) from this seed and have the same length as DR Message Key
	 */
	const std::string hkdf_randomSeed_info{"DR Message Key Derivation"};

	/// DR Public key index size is 12 bytes long (used to identify a DR reception chain for KEM based DR)
	/// it is a hash of the key, on 96 bits, collision chances are negligible
	constexpr size_t DRPkIndexSize=12;

	/// AEAD generates tag 16 bytes long
	constexpr size_t DRMessageAuthTagSize=16;

/******************************************************************************/
/*                                                                            */
/* Local Storage related definitions                                          */
/*                                                                            */
/******************************************************************************/
	/** define a version number for the DB schema as an integer 0xMMmmpp
	 *
	 * current version is 0.2.0
	 */
	constexpr int DBuserVersion=0x000200;
	constexpr uint16_t DBInactiveUserBit = 0x0100;
	constexpr uint16_t DBCurveIdByte = 0x00FF;
	constexpr uint8_t DBInvalidIk = 0x00;

/******************************************************************************/
/*                                                                            */
/* X3DH related definitions                                                   */
/*                                                                            */
/******************************************************************************/
	/// shall be an ASCII string identifying the application (X3DH spec section 2.1)
	const std::string X3DH_SK_info{"Lime"};
	/// used to generate a shared AD based on Ik and deviceID
	const std::string X3DH_AD_info{"X3DH Associated Data"};
} // namespace settings

} // namespace lime

#endif /* lime_defines_hpp */
