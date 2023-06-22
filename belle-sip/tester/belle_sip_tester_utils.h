/*
 * Copyright (c) 2012-2023 Belledonne Communications SARL.
 *
 * This file is part of belle-sip.
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

#ifndef BELLE_SIP_TESTER_UTILS_H
#define BELLE_SIP_TESTER_UTILS_H

#include "belle-sip/belle-sip.h"

#ifdef __cplusplus
extern "C" {
#endif

bool_t belle_sip_is_executable_installed(const char *executable, const char *resource);
void belle_sip_add_belr_grammar_search_path(const char *path);

#ifdef __cplusplus
}
#endif

#endif // BELLE_SIP_TESTER_UTILS_H
