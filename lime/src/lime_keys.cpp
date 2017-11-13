/*
	lime_keys.cpp
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define BCTBX_LOG_DOMAIN "lime"
#include <bctoolbox/logging.h>

#include "lime_keys.hpp"

namespace lime {

	/****************************************************************/
	/* ECDH keys                                                    */
	/****************************************************************/
	/* function to create a ECDH context */
	template <typename Curve>
	bctbx_ECDHContext_t *ECDHInit(void) {
		/* if this template is instanciated the static_assert will fail but will give us an error message with faulty Curve type */
		static_assert(sizeof(Curve) != sizeof(Curve), "You must specialize sessionsInit<> for your type: correctly initialise the ECDH context");
		return nullptr;
	}

#ifdef EC25519_ENABLED
	/* specialise ECDH context creation */
	template <> bctbx_ECDHContext_t *ECDHInit<C255>(void) {
		return bctbx_CreateECDHContext(BCTBX_ECDH_X25519);
	}
#endif

#ifdef EC448_ENABLED
	/* specialise ECDH context creation */
	template <> bctbx_ECDHContext_t *ECDHInit<C448>(void) {
		return bctbx_CreateECDHContext(BCTBX_ECDH_X448);
	}
#endif

	/****************************************************************/
	/* EdDSA keys                                                   */
	/****************************************************************/
	/* function to create a EDDSA context */
	template <typename Curve>
	bctbx_EDDSAContext_t *EDDSAInit(void) {
		/* if this template is instanciated the static_assert will fail but will give us an error message with DRType */
		static_assert(sizeof(Curve) != sizeof(Curve), "You must specialize sessionsInit<> for your type: correctly initialise the ECDH context");
		return nullptr;
	}

#ifdef EC25519_ENABLED
	/* specialise EDDSA context creation */
	template <> bctbx_EDDSAContext_t *EDDSAInit<C255>(void) {
		return bctbx_CreateEDDSAContext(BCTBX_EDDSA_25519);
	}
#endif

#ifdef EC448_ENABLED
	/* specialise EDDSA context creation */
	template <> bctbx_EDDSAContext_t *EDDSAInit<C448>(void) {
		return bctbx_CreateEDDSAContext(BCTBX_EDDSA_448);
	}
#endif

/* template instanciations for Curves 25519 and 448, done  */
#ifdef EC255_ENABLED
	template class X<C255>;
	template class ED<C255>;
	template class KeyPair<X<C255>>;
	template class KeyPair<ED<C255>>;
#endif

#ifdef EC448_ENABLED
	template class X<C448>;
	template class ED<C448>;
	template class KeyPair<X<C448>>;
	template class KeyPair<ED<C448>>;
#endif
}
