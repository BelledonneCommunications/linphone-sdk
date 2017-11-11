/*
	lime_utils.hpp
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

#ifndef lime_utils_hpp
#define lime_utils_hpp

namespace lime {
// this namespace hold constants definition used as settings in all components of the lime library
namespace settings {
/******************************************************************************/
/*                                                                            */
/* Generic settings: number of OPks to generate, Life time of SPks, ecc..     */
/*                                                                            */
/******************************************************************************/
	constexpr uint16_t OPk_batch_number = 5; //TODO: 5 is ok for testing purpose what to set on real deployment

/******************************************************************************/
/*                                                                            */
/* Double Ratchet related definitions                                         */
/*                                                                            */
/******************************************************************************/

	// Sending, Receiving and Root key chain use 32 bytes keys (spec 3.2)
	constexpr size_t DRChainKeySize=32;

	// Message Key are composed of a 32 bytes key and 16 bytes of IV
	constexpr size_t DRMessageKeySize=32;
	constexpr size_t DRMessageIVSize=16;

	// AEAD generates tag 16 bytes long
	constexpr size_t DRMessageAuthTagSize=16;

	// Each session stores a shared AD given at built and derived from Identity keys of sender and receiver
	// SharedAD is computed by X3DH HKDF(session Initiator Ik || session receiver Ik || session Initiator device Id || session receiver device Id)
	constexpr size_t DRSessionSharedADSize=32;

	// Maximum number of Message we can skip(and store their keys) at reception of one message
	constexpr std::uint16_t maxMessageSkip=1024;

	// Maximum length of Sending chain: is this count is reached without any return from peer,
	// the DR session is set to stale and we must create another one to send messages
	// Can't be more than 2^16 as message number is send on 2 bytes
	constexpr std::uint16_t maxSendingChain=1000;

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
	const std::string X3DH_AD_info{"X3DH Authenticated Data"}; // used to generate a shared AD based on Ik and deviceID
}

}

#endif /* lime_utils_hpp */
