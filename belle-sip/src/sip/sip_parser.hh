/*
 * Copyright (c) 2012-2024 Belledonne Communications SARL.
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

#include <belr/abnf.h>
#include <belr/grammarbuilder.h>

#include "belle-sip/types.h"

using namespace std;

namespace bellesip {
namespace SIP {
class Parser {
public:
	static Parser *getInstance();
	void *parse(const string &input, const string &rule, size_t *parsedSize, bool fullMatch = false);

private:
	static Parser *instance;
	Parser();
	~Parser();
	shared_ptr<belr::Grammar> loadGrammar();
	shared_ptr<belr::Parser<void *>> _parser;
};
} // namespace SIP
} // namespace bellesip

typedef struct {
	belle_sip_object_t base;
	void *obj;
} belle_sip_parser_context_t;

belle_sip_parser_context_t *belle_sip_parser_context_new(void);
void belle_sip_parser_context_add_header(belle_sip_parser_context_t *context, belle_sip_header_t *header);
void belle_sip_parser_context_add_header_check_uri(belle_sip_parser_context_t *context, belle_sip_header_t *header);
void belle_sip_parser_context_add_extension_header(belle_sip_parser_context_t *context, belle_sip_header_t *header);
void belle_sip_parser_context_add_header_from_parser_context(belle_sip_parser_context_t *context,
                                                             belle_sip_parser_context_t *context_with_header);
void belle_sip_parser_context_set_message(belle_sip_parser_context_t *context, belle_sip_message_t *message);

#define BELLE_SIP_PARSER_CONTEXT(t) BELLE_SIP_CAST(t, belle_sip_parser_context_t)

void belle_sip_message_add_header_from_parser_context(belle_sip_message_t *message,
                                                      belle_sip_parser_context_t *context);
