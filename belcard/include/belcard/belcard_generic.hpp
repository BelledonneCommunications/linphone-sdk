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

#include <memory>
#include <sstream>

using namespace::std;

namespace belcard {
	class BelCardGeneric {
	public:
		template<typename T>
		static shared_ptr<T> create() {
			return make_shared<T>();
		}
		
		BelCardGeneric() { }
		virtual ~BelCardGeneric() { } // A virtual destructor enables polymorphism and dynamic casting.
		
		virtual void serialize(ostream &output) const = 0; // Force heriting classes to define this
		
		friend ostream &operator<<(ostream &output, const BelCardGeneric &me) {
			me.serialize(output);
			return output;
		}
		
		virtual string toString() const {
			stringstream output;
			output << *this;
			return output.str();
		}
	};
}

#endif