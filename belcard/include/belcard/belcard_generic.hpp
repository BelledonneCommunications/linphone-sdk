#ifndef belcard_generic_hpp
#define belcard_generic_hpp

#include "belcard/vcard_grammar.hpp"

#include <memory>

using namespace::std;

namespace belcard {
	class BelCardGeneric {
	public:
		template<typename T>
		static shared_ptr<T> create() {
			return make_shared<T>();
		}
		
		BelCardGeneric() { }
		
		virtual ~BelCardGeneric() { } //put a virtual destructor to enable polymorphism and dynamic casting.
	};
}

#endif