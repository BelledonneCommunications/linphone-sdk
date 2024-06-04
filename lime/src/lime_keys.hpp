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
	/** @brief List of data types used by key Echange algorithm
	 *
	 * public key, private key and shared secret
	 * */
	enum class Xtype {publicKey, privateKey, sharedSecret};
	/** @brief List of data types used by key encapsulation mechanism algorithm
	 *
	 * public key, private key, cipher text and shared secret
	 * */
	enum class Ktype {publicKey, privateKey, cipherText, sharedSecret};

	/** @brief List of data types used by Signature algorithm
	 *
	 * public key, private key and signature
	 * */
	enum class DSAtype {publicKey, privateKey, signature};

	/* define needed constant for the curves: self identificatio(used in DB and as parameter from lib users, data structures sizes)*/
	/* These structure are used as template argument to enable support for different key Exchznge and signature Algorithms */

	/**
	 *  @brief curve 25519 data types size definition
	 *
	 *  use a 4 chars(C255) to identify it to improve code readability
	 */
	struct C255 {
		using EC = C255;
		/// a string to indentify this curve
		static const std::string Id() {return std::string("CURVE25519");};
		/// the C25519 curve id using the CurveId enumeration
		static constexpr lime::CurveId curveId() {return lime::CurveId::c25519;};
		/// for X25519, public, private and shared secret have the same length: 32 bytes
		static constexpr size_t Xsize(lime::Xtype dataType) {return 32;};
		/// for Ed25519, public and private key have the same length: 32 bytes, signature is 64 bytes long
		static constexpr size_t DSAsize(lime::DSAtype dataType) {return (dataType != lime::DSAtype::signature)?32:64;};
	};

	/**
	 * @brief curve 448-goldilocks data types size definition
	 */
	struct C448 {
		using EC = C448;
		/// a string to indentify this curve
		static const std::string Id() {return std::string("CURVE448");};
		 ///the C448 curve id using the CurveId enumeration
		static constexpr lime::CurveId curveId() {return lime::CurveId::c448;};
		/// for X448, public, private and shared secret have the same length 56 bytes
		static constexpr size_t Xsize(lime::Xtype dataType) {return 56;};
		/// for Ed448, public and private key have the same length 57 bytes, signature is 114 bytes long
		static constexpr size_t DSAsize(lime::DSAtype dataType) {return (dataType != lime::DSAtype::signature)?57:114;};
	};

	struct genericKEM {
		static constexpr lime::CurveId curveId() {return lime::CurveId::unset;}; // KEM cannot be used directly in DR or X3DH -> no curveId
	};
#ifdef HAVE_BCTBXPQ
	/**
	 * @brief Kyber 512 KEM data types size definition
	 */
	struct K512: public genericKEM {
		/// a string to indentify this curve
		static const std::string Id(void) {return std::string("KYBER512");};
		 /// kyber512
		static constexpr size_t Ksize(lime::Ktype dataType) {
			switch (dataType) {
				case lime::Ktype::publicKey:
					return 800;
				break;
				case lime::Ktype::privateKey:
					return 1632;
				break;
				case lime::Ktype::cipherText:
					return 768;
				break;
				case lime::Ktype::sharedSecret:
					return 32;
				break;
			}
			return 0; // make compiler happy
		};
	};

	struct C255K512: public C255,K512 {
		static constexpr lime::CurveId curveId(void) {return lime::CurveId::c25519k512;};
		using EC = C255;
		using KEM = K512;
	};
#endif // HAVE_BCTBXPQ

	// Hash function defines
	/**
	 * @brief SHA512 buffer size definition
	 */
	struct SHA512 {
		/// maximum output size for SHA512 is 64 bytes
		static constexpr size_t ssize() {return 64;}
	};

	// AEAD function defines
	/**
	 * @brief AES256GCM buffers size definition
	 */
	struct AES256GCM {
		/// key size is 32 bytes
		static constexpr size_t keySize(void) {return 32;};
		/// we use authentication tag size of 16 bytes
		static constexpr size_t tagSize(void) {return 16;};
	};
}

#endif /* lime_keys_hpp */
