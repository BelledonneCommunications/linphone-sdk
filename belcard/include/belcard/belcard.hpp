#ifndef belcard_hpp
#define belcard_hpp

#include "belcard_generic.hpp"
#include "belcard_general.hpp"
#include "belcard_identification.hpp"
#include "belcard_addressing.hpp"
#include "belcard_communication.hpp"
#include "belcard_geographical.hpp"
#include "belcard_organizational.hpp"
#include "belcard_explanatory.hpp"
#include "belcard_security.hpp"
#include "belcard_calendar.hpp"

#include <string>
#include <list>

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCard : public BelCardGeneric {
	private:
		shared_ptr<BelCardKind> _kind;
		shared_ptr<BelCardFN> _fn;
		shared_ptr<BelCardN> _n;
		shared_ptr<BelCardBirthday> _bday;
		shared_ptr<BelCardAnniversary> _anniversary;
		shared_ptr<BelCardGender> _gender;
		list<shared_ptr<BelCardNickname>> _nicknames;
		list<shared_ptr<BelCardPhoto>> _photos;
		list<shared_ptr<BelCardAddress>> _addr;
		list<shared_ptr<BelCardTel>> _tel;
		list<shared_ptr<BelCardEmail>> _emails;
		list<shared_ptr<BelCardImpp>> _impp;
		list<shared_ptr<BelCardLang>> _langs;
		list<shared_ptr<BelCardSource>> _sources;
		list<shared_ptr<BelCardXML>> _xml;
		list<shared_ptr<BelCardProperty>> _properties;
		
	public:
		static shared_ptr<BelCard> create();
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCard();

		friend ostream &operator<<(ostream &output, const BelCard &card) {
			output << card.serialize();
			return output;
		}
		
		void setKind(const shared_ptr<BelCardKind> &kind);
		const shared_ptr<BelCardKind> &getKind() const;
		
		void setFN(const shared_ptr<BelCardFN> &fn);
		const shared_ptr<BelCardFN> &getFN() const;
		
		void setN(const shared_ptr<BelCardN> &n);
		const shared_ptr<BelCardN> &getN() const;
		
		void setBirthday(const shared_ptr<BelCardBirthday> &bday);
		const shared_ptr<BelCardBirthday> &getBirthday() const;
		
		void setAnniversary(const shared_ptr<BelCardAnniversary> &anniversary);
		const shared_ptr<BelCardAnniversary> &getAnniversary() const;
		
		void setGender(const shared_ptr<BelCardGender> &gender);
		const shared_ptr<BelCardGender> &getGender() const;
		
		void addNickname(const shared_ptr<BelCardNickname> &nickname);
		const list<shared_ptr<BelCardNickname>> &getNicknames() const;
		
		void addPhoto(const shared_ptr<BelCardPhoto> &photo);
		const list<shared_ptr<BelCardPhoto>> &getPhotos() const;
		
		void addAddress(const shared_ptr<BelCardAddress> &addr);
		const list<shared_ptr<BelCardAddress>> &getAddresses() const;
		
		void addTel(const shared_ptr<BelCardTel> &tel);
		const list<shared_ptr<BelCardTel>> &getTel() const;
		
		void addEmail(const shared_ptr<BelCardEmail> &email);
		const list<shared_ptr<BelCardEmail>> &getEmails() const;
		
		void addImpp(const shared_ptr<BelCardImpp> &impp);
		const list<shared_ptr<BelCardImpp>> &getImpp() const;
		
		void addLang(const shared_ptr<BelCardLang> &lang);
		const list<shared_ptr<BelCardLang>> &getLangs() const;
		
		void addSource(const shared_ptr<BelCardSource> &source);
		const list<shared_ptr<BelCardSource>> &getSource() const;
		
		void addXML(const shared_ptr<BelCardXML> &xml);
		const list<shared_ptr<BelCardXML>> &getXML() const;
		
		void addProperty(const shared_ptr<BelCardProperty> &property);
		const list<shared_ptr<BelCardProperty>> &getProperties() const;
		
		string serialize() const;
	};	
}

#endif