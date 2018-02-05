/*
	lime_keys.hpp
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

#ifndef lime_keys_hpp
#define lime_keys_hpp

#include <algorithm> //std::copy_n
#include <array>
#include <iterator>
#include "lime/lime.hpp"

namespace lime {
	// Data structure type enumerations
	enum class Xtype {publicKey, privateKey, sharedSecret}; // key Echange has public key, private key and shared secret
	enum class DSAtype {publicKey, privateKey, signature}; // Signature has public key, private key and signature

	/* define needed constant for the curves: self identificatio(used in DB and as parameter from lib users, data structures sizes)*/
	/* These structure are used as template argument to enable support for different Algorithms */
	struct C255 { // curve 25519, use a 4 chars to identify it to improve code readability
		static constexpr lime::CurveId curveId() {return lime::CurveId::c25519;};
		// for X25519, public, private and shared secret have the same length: 32 bytes
		static constexpr size_t Xsize(lime::Xtype dataType) {return 32;};
		// for Ed25519, public and private key have the same length: 32 bytes, signature is 64 bytes long
		static constexpr size_t DSAsize(lime::DSAtype dataType) {return (dataType != lime::DSAtype::signature)?32:64;};
	};

	struct C448 { // curve 448-goldilocks
		static constexpr lime::CurveId curveId() {return lime::CurveId::c448;};
		// for X448, public, private and shared secret have the same length 56 bytes
		static constexpr size_t Xsize(lime::Xtype dataType) {return 56;};
		// for Ed448, public and private key have the same length 57 bytes, signature is 114 bytes long
		static constexpr size_t DSAsize(lime::DSAtype dataType) {return (dataType != lime::DSAtype::signature)?57:114;};
	};
}

#endif /* lime_keys_hpp */
