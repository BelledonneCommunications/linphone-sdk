/*
    belcard_general.hpp
    Copyright (C) 2015  Belledonne Communications SARL

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

#ifndef belcard_general_hpp
#define belcard_general_hpp

#include "belcard_property.hpp"
#include "belcard_utils.hpp"
#include <belr/abnf.h>
#include <belr/grammarbuilder.h>

#include <string>

namespace belcard {
class BelCardKind : public BelCardProperty {
public:
	BELCARD_PUBLIC static std::shared_ptr<BelCardKind> parse(const std::string &input, bool v3);
	BELCARD_PUBLIC static void setHandlerAndCollectors(belr::Parser<std::shared_ptr<BelCardGeneric>> *parser,
	                                                   bool v3 = false);

	BELCARD_PUBLIC BelCardKind(bool v3);
};

class BelCardSource : public BelCardProperty {
public:
	BELCARD_PUBLIC static std::shared_ptr<BelCardSource> parse(const std::string &input, bool v3);
	BELCARD_PUBLIC static void setHandlerAndCollectors(belr::Parser<std::shared_ptr<BelCardGeneric>> *parser,
	                                                   bool v3 = false);

	BELCARD_PUBLIC BelCardSource(bool v3);
};

class BelCardXML : public BelCardProperty {
public:
	BELCARD_PUBLIC static std::shared_ptr<BelCardXML> parse(const std::string &input, bool v3);
	BELCARD_PUBLIC static void setHandlerAndCollectors(belr::Parser<std::shared_ptr<BelCardGeneric>> *parser,
	                                                   bool v3 = false);

	BELCARD_PUBLIC BelCardXML(bool v3);
};
} // namespace belcard

#endif
