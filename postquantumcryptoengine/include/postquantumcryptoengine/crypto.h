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

#ifndef POSTQUANTUMCRYPTO_H
#define POSTQUANTUMCRYPTO_H

#ifdef _MSC_VER
	#ifdef PQCE_STATIC
		#define PQCE_EXPORT
	#else /* PQCE_STATIC */
		#ifdef PQCE_EXPORTS
			#define PQCE_EXPORT __declspec(dllexport)
		#else /* PQCE_EXPORTS */
			#define PQCE_EXPORT __declspec(dllimport)
		#endif /* PQCE_EXPORTS */
	#endif /* PQCE_STATIC */

	#ifndef PQCE_DEPRECATED
		#define PQCE_DEPRECATED __declspec(deprecated)
	#endif /* PQCE_DEPRECATED */
#else /* _MSC_VER*/
	#define PQCE_EXPORT __attribute__ ((visibility ("default")))

	#ifndef PQCE_DEPRECATED
		#define PQCE_DEPRECATED __attribute__ ((deprecated))
	#endif /* PQCE_DEPRECATED */
#endif /* _MSC_VER*/

#ifdef __cplusplus
extern "C" {
#endif

PQCE_EXPORT uint32_t bctbxpq_key_agreement_algo_list(void);

#ifdef __cplusplus
}
#endif
#endif // POSTQUANTUMCRYPTO_H

