#ifndef belcard_hpp
#define belcard_hpp

#include "belcard_generic.hpp"
#include "belcard_identification.hpp"

#include <string>
#include <list>
#include <map>
#include <memory>

using namespace::std;

namespace belcard {
	class BelCard : public BelCardGeneric {
	private:
		shared_ptr<BelCardFN> _fn;
		shared_ptr<BelCardN> _n;
		list<shared_ptr<BelCardNickname>> _nicknames;
		list<shared_ptr<BelCardProperty>> _properties;
		
	public:
		static shared_ptr<BelCard> create() {
			return make_shared<BelCard>();
		}
		
		BelCard() {
			
		}
		
		void setFN(const shared_ptr<BelCardFN> &fn) {
			_fn = fn;
			addProperty(_fn);
		}
		const shared_ptr<BelCardFN> &getFN() const {
			return _fn;
		}
		
		void setN(const shared_ptr<BelCardN> &n) {
			_n = n;
			addProperty(_n);
		}
		const shared_ptr<BelCardN> &getN() const {
			return _n;
		}
		
		void addNickname(const shared_ptr<BelCardNickname> &nickname) {
			_nicknames.push_back(nickname);
			addProperty(nickname);
		}
		const list<shared_ptr<BelCardNickname>> &getNicknames() const {
			return _nicknames;
		}
		
		void addProperty(const shared_ptr<BelCardProperty> &property) {
			_properties.push_back(property);
		}
		const list<shared_ptr<BelCardProperty>> &getProperties() const {
			return _properties;
		}
		
		string toString() {
			string vcard = "BEGIN:VCARD\r\nVERSION:4.0\r\n";
			for (auto it = _properties.begin(); it != _properties.end(); ++it) {
				vcard += (*it)->toString(); 
			}
			vcard += "END:VCARD\r\n";
			return vcard;
		}
	};	
}

#endif