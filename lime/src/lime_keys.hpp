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
#include "bctoolbox/crypto.h"
#include "lime/lime.hpp"

namespace lime {
	// define available keys and signatures sizes in an enum (forward define from bctoolbox/crypto.h)
	enum class keySize : size_t {x25519=BCTBX_ECDH_X25519_PUBLIC_SIZE, x448=BCTBX_ECDH_X448_PUBLIC_SIZE, ed25519=BCTBX_EDDSA_25519_PUBLIC_SIZE, ed448=BCTBX_EDDSA_448_PUBLIC_SIZE};
	enum class sigSize : size_t {ed25519=BCTBX_EDDSA_25519_SIGNATURE_SIZE, ed448=BCTBX_EDDSA_448_SIGNATURE_SIZE};

	/* define needed constant for the curves: self identificatio(used in DB and as parameter from lib users, keys sizes)*/
	/* These structure are used as template argument to enable support for different EC. Some templates specialisation MUST be define in lime_keys.cpp to be able to use them */
	struct C255 { // curve 25519, use a 4 chars to identify it to improve code readability
		static constexpr lime::CurveId curveId() {return lime::CurveId::c25519;};
		static constexpr size_t XkeySize() {return static_cast<size_t>(keySize::x25519);};
		static constexpr size_t EDkeySize() {return static_cast<size_t>(keySize::ed25519);};
		static constexpr size_t EDSigSize() {return static_cast<size_t>(sigSize::ed25519);};
	};

	struct C448 { // curve 448-goldilocks
		static constexpr lime::CurveId curveId() {return lime::CurveId::c448;};
		static constexpr size_t XkeySize() {return static_cast<size_t>(keySize::x448);};
		static constexpr size_t EDkeySize() {return static_cast<size_t>(keySize::ed448);};
		static constexpr size_t EDSigSize() {return static_cast<size_t>(sigSize::ed448);};
	};


	/****************************************************************/
	/* ECDH keys                                                    */
	/****************************************************************/
	/* function to create a ECDH context */
	template <typename Curve>
	bctbx_ECDHContext_t *ECDHInit(void);

	template <typename Curve>
	class X : public std::array<uint8_t, static_cast<size_t>(Curve::XkeySize())>{
		public :
			constexpr static size_t keyLength(void) {return Curve::XkeySize();}; // provide a static size function to be able to call the function not on an object
			// construct from a C style buffer
			// WARNING: very dangerous code could lead to read anywhere(X{0} will call this constructor), get rid of it if we manage to use c++ buffer style only - get a bctoolbox/crypto.hpp?
			X(const uint8_t *buffer) {std::copy_n(buffer, Curve::XkeySize(), this->data());}
			X(std::vector<uint8_t>::const_iterator buffer) {std::copy_n(buffer, Curve::XkeySize(), this->begin());} // construct from a std::vector<uint8_t>
			X() {};
			void assign(std::vector<uint8_t>::const_iterator buffer) {std::copy_n(buffer, Curve::XkeySize(), this->begin());} // copy from a std::vector<uint8_t>
	};

	/****************************************************************/
	/* EdDSA keys and signature                                     */
	/****************************************************************/
	/* function to create a ECDH context */
	template <typename Curve>
	bctbx_EDDSAContext_t *EDDSAInit(void);

	template <typename Curve>
	class ED : public std::array<uint8_t, static_cast<size_t>(Curve::EDkeySize())>{
		public :
			constexpr static size_t keyLength(void) {return Curve::EDkeySize();}; // provide a static size function to be able to call the function not on an object
			// construct from a C style buffer
			// WARNING: very dangerous code could lead to read anywhere(ED{0} will call this constructor), get rid of it if we manage to use c++ buffer style only - get a bctoolbox/crypto.hpp?
			ED(const uint8_t *buffer) {std::copy_n(buffer, Curve::EDkeySize(), this->data());}
			ED(std::vector<uint8_t>::const_iterator buffer) {std::copy_n(buffer, Curve::EDkeySize(), this->begin());} // contruct from a std::vector<uint8_t>
			ED() {};
			void assign(std::vector<uint8_t>::const_iterator buffer) {std::copy_n(buffer, Curve::EDkeySize(), this->begin());} // copy from a std::vector<uint8_t>
	};

	template <typename Curve>
	class Signature : public std::array<uint8_t, static_cast<size_t>(Curve::EDSigSize())>{
		public :
			constexpr static size_t signatureLength(void) {return Curve::EDSigSize();}; // provide a static size function to be able to call the function not on an object
			Signature(const uint8_t *buffer) {std::copy_n(buffer, Curve::EDSigSize(), this->data());}
			Signature(std::vector<uint8_t>::const_iterator buffer) {std::copy_n(buffer, Curve::EDSigSize(), this->begin());}
			Signature() {};
			void assign(std::vector<uint8_t>::const_iterator buffer) {std::copy_n(buffer, Curve::EDSigSize(), this->begin());} // copy from a std::vector<uint8_t>
	};


	/****************************************************************/
	/* Common structure: key pair                                   */
	/****************************************************************/
	template <typename Key>
	class KeyPair {
		private:
			/* for all supported algo(X25519, X448, ED25519, ED448), private and public keys have the same size, so we use the same type to declare them in a pair */
			Key _pubKey;
			Key _privKey;
		public:
			size_t size(void) {return _pubKey.size();};  // by construction private and public are of the same type so have same size so we can return any size fron this two keys.
			Key &privateKey(void) {return _privKey;};
			Key &publicKey(void) {return _pubKey;};
			KeyPair(Key pub, Key priv):_pubKey(pub),_privKey(priv) {};
			KeyPair(uint8_t *pub, uint8_t *priv):_pubKey{pub},_privKey{priv} {};
			KeyPair() :_pubKey{},_privKey{}{};
			bool operator==(KeyPair<Key> b) const {return (_privKey==b.privateKey() && _pubKey==b.publicKey());};
	};

	/****************************************************************/
	/* Template are instanciated in lime_keys.cpp                   */
	/* Do not re-instiate them elsewhere                            */
	/****************************************************************/
#ifdef EC25519_ENABLED
	/* declare specialisation for C255 */
	template <> bctbx_ECDHContext_t *ECDHInit<C255>(void);
	template <> bctbx_EDDSAContext_t *EDDSAInit<C255>(void);
	/* ask any file including lime_keys.hpp to not instantiate the follownings as it is done in lime_keys.cpp*/
	extern template class X<C255>;
	extern template class ED<C255>;
	extern template class KeyPair<X<C255>>;
	extern template class KeyPair<ED<C255>>;
#endif

#ifdef EC448_ENABLED
	/* declare specialisation for C488 */
	template <> bctbx_ECDHContext_t *ECDHInit<C448>(void);
	template <> bctbx_EDDSAContext_t *EDDSAInit<C448>(void);
	/* ask any file including lime_keys.hpp to not instantiate the follownings as it is done in lime_keys.cpp*/
	extern template class X<C448>;
	extern template class ED<C448>;
	extern template class KeyPair<X<C448>>;
	extern template class KeyPair<ED<C448>>;
#endif

}

#endif /* lime_keys_hpp */
