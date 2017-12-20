/*
 * Copyright (C) 2017  Belledonne Communications SARL
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _ABNF_H_
#define _ABNF_H_

#include "belr.h"

// =============================================================================

namespace belr{

/**
 * Grammar containing core definitions of ABNF.
 * This is required for almost all IETF text based protocols.
**/
class CoreRules : public Grammar{
public:
	///Initialize a CoreRules grammar object.
	BELR_PUBLIC CoreRules();
private:
	void alpha();
	void bit();
	void char_();
	void cr();
	void lf();
	void crlf();
	void ctl();
	void digit();
	void dquote();
	void hexdig();
	void htab();
	void lwsp();
	void octet();
	void sp();
	void vchar();
	void wsp();
};



class ABNFGrammar : public Grammar{
public:
	ABNFGrammar();
private:
	void comment();
	void c_nl();
	void c_wsp();
	void rulename();
	void repeat();
	void repeat_min();
	void repeat_max();
	void repeat_count();
	void defined_as();
	void rulelist();
	void rule();
	void elements();
	void alternation();
	void concatenation();
	void repetition();
	void element();
	void group();
	void option();
	void char_val();
	void num_val();
	void prose_val();
	void bin_val();
	void dec_val();
	void hex_val();
	void crlf_or_lf();
};


}
#endif
