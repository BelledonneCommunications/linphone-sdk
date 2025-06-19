/*
 * Copyright (c) 2014-2019 Belledonne Communications SARL.
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

#ifndef postquantumcryptoengineTester_h
#define postquantumcryptoengineTester_h

#define BCTOOLBOX_LOG_DOMAIN "pqc-tester"
#include <bctoolbox/logging.h>

#include <bctoolbox/tester.h>

#ifdef __cplusplus
extern "C"{
#endif

extern test_suite_t crypto_test_suite;

extern int verbose;

#ifdef __cplusplus
};
#endif

#endif // postquantumcryptoTester_h
