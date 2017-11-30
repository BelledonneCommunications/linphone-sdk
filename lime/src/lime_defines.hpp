/*
	lime_defines.hpp
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

#ifndef lime_defines_hpp
#define lime_defines_hpp

namespace lime {
// this namespace hold constants definition used as settings in all components of the lime library
// the constants defined in this file cannot be modified without some work on the source code
// unless you really know what you're doing, just leave them alone
namespace settings {

/******************************************************************************/
/*                                                                            */
/* Double Ratchet related definitions                                         */
/*                                                                            */
/******************************************************************************/

	// Sending, Receiving and Root key chain use 32 bytes keys (spec 3.2)
	constexpr size_t DRChainKeySize=32;

	// DR Message Key are composed of a 32 bytes key and 16 bytes of IV
	constexpr size_t DRMessageKeySize=32;
	constexpr size_t DRMessageIVSize=16;

	// Message Key is based on a message seed(sent in the DR message)
	// Message key and nonce are derived from this seed and have the same length as DR Message Key
	constexpr size_t DRrandomSeedSize=32;
	const std::string hkdf_randomSeed_info{"DR Message Key Derivation"};

	// AEAD generates tag 16 bytes long
	constexpr size_t DRMessageAuthTagSize=16;

/******************************************************************************/
/*                                                                            */
/* Local Storage related definitions                                          */
/*                                                                            */
/******************************************************************************/
	/* define a version number for the DB schema as an integer 0xMMmmpp */
	/* current version is 0.0.1 */
	constexpr int DBuserVersion=0x000001;

/******************************************************************************/
/*                                                                            */
/* X3DH related definitions                                                   */
/*                                                                            */
/******************************************************************************/
	const std::string X3DH_SK_info{"Lime"}; // shall be an ASCII string identifying the application (X3DH spec section 2.1)
	const std::string X3DH_AD_info{"X3DH Associated Data"}; // used to generate a shared AD based on Ik and deviceID
} // namespace settings

} // namespace lime

#endif /* lime_defines_hpp */
