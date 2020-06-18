/*
 * Copyright (c) 2020 Belledonne Communications SARL.
 *
 * This file is part of bctoolbox.
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

#ifndef BCTBX_CRYPTO_HH
#define BCTBX_CRYPTO_HH

#include <vector>
#include <memory>

namespace bctoolbox {
/**
 * @brief Random number generator interface
 *
 * This wrapper provides an interface to a RNG.
 * Two ways to get some random numbers:
 *  - calling the static class functions(c_randomize) : do not use this to feed cryptographic functions
 *  - instanciate a RNG object and call the randomize method : use this one for cryptographic quality random
 *
 * Any call (including creation), may throw an exception if some error are detected on the random source
 */
class RNG {
	public:
		/**
		 * fill a buffer with random numbers
		 * @param[in,out]	buffer 	The buffer to be filled with random (callers responsability to allocate memory)
		 * @param[in]		size	size in bytes of the random generated, buffer must be at least of this size
		 **/
		void randomize(uint8_t *buffer, const size_t size);

		/**
		 * generates a 32 bits random unsigned number
		 **/
		uint32_t randomize();

		/**
		 * fill a buffer with random numbers
		 * @param[in,out]	buffer 	The buffer to be filled with random (callers responsability to allocate memory)
		 * @param[in]		size	size in bytes of the random generated, buffer must be at least of this size
		 *
		 * @note This function uses a shared RNG context, do not use it to generate sensitive material
		 **/
		static void c_randomize(uint8_t *buffer, size_t size);
		/**
		 * generates a 32 bits random unsigned number
		 *
		 * @note This function uses a shared RNG context, do not use it to generate sensitive material
		 **/
		static uint32_t c_randomize();

		RNG();
		~RNG();
	private:
		struct Impl;
		std::unique_ptr<Impl> pImpl;
		static std::unique_ptr<Impl> pImplClass;
}; //class RNG

} // namespace bctoolbox
#endif // BCTBX_CRYPTO_HH


