/*
    belcard_generic.hpp
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

#ifndef belcard_generic_hpp
#define belcard_generic_hpp

#include "belcard/vcard_grammar.hpp"
#include "belcard_utils.hpp"

#include <memory>
#include <sstream>

namespace belcard {
class BelCardGeneric {
	friend class BelCardParser;

protected:
	bool _v3 = false;

public:
	template <typename T>
	static std::shared_ptr<T> create() {
		return std::make_shared<T>(false);
	}

	template <typename T>
	static std::shared_ptr<T> createV3() {
		return std::make_shared<T>(true);
	}

	template <typename T>
	static bool isValid(const std::shared_ptr<T> &property) {
		return T::parse(property->toString(), property->isUsingV3Grammar()) != nullptr;
	}

	BELCARD_PUBLIC BelCardGeneric(bool v3) {
		_v3 = v3;
	}
	BELCARD_PUBLIC virtual ~BelCardGeneric() = default;

	BELCARD_PUBLIC virtual void serialize(std::ostream &output) const = 0; // Force heriting classes to define this

	BELCARD_PUBLIC friend std::ostream &operator<<(std::ostream &output, const BelCardGeneric &me) {
		me.serialize(output);
		return output;
	}

	BELCARD_PUBLIC virtual std::string toString() const {
		std::stringstream output;
		output << *this;
		return output.str();
	}

	BELCARD_PUBLIC bool isUsingV3Grammar() const {
		return _v3;
	}
};
} // namespace belcard

#endif
