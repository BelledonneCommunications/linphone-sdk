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