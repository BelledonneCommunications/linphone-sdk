#ifndef belcard_hpp
#define belcard_hpp

#include "belcard_generic.hpp"
#include "belcard_general.hpp"
#include "belcard_identification.hpp"
#include "belcard_addressing.hpp"
#include "belcard_communication.hpp"

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
		static shared_ptr<BelCard> create() {
			return make_shared<BelCard>();
		}
		
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
			parser->setHandler("vcard", make_fn(&BelCard::create))
					->setCollector("KIND", make_sfn(&BelCard::setKind))
					->setCollector("FN", make_sfn(&BelCard::setFN))
					->setCollector("N", make_sfn(&BelCard::setN))
					->setCollector("BDAY", make_sfn(&BelCard::setBirthday))
					->setCollector("ANNIVERSARY", make_sfn(&BelCard::setAnniversary))
					->setCollector("GENDER", make_sfn(&BelCard::setGender))
					->setCollector("NICKNAME", make_sfn(&BelCard::addNickname))
					->setCollector("PHOTO", make_sfn(&BelCard::addPhoto))
					->setCollector("ADR", make_sfn(&BelCard::addAddress))
					->setCollector("TEL", make_sfn(&BelCard::addTel))
					->setCollector("EMAIL", make_sfn(&BelCard::addEmail))
					->setCollector("IMPP", make_sfn(&BelCard::addImpp))
					->setCollector("LANG", make_sfn(&BelCard::addLang))
					->setCollector("SOURCE", make_sfn(&BelCard::addSource))
					->setCollector("XML", make_sfn(&BelCard::addXML));
		}
		
		BelCard() {
			
		}
		
		void setKind(const shared_ptr<BelCardKind> &kind) {
			_kind = kind;
			addProperty(_kind);
		}
		const shared_ptr<BelCardKind> &getKind() const {
			return _kind;
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
		
		void setBirthday(const shared_ptr<BelCardBirthday> &bday) {
			_bday = bday;
			addProperty(_bday);
		}
		const shared_ptr<BelCardBirthday> &getBirthday() const {
			return _bday;
		}
		
		void setAnniversary(const shared_ptr<BelCardAnniversary> &anniversary) {
			_anniversary = anniversary;
			addProperty(_anniversary);
		}
		const shared_ptr<BelCardAnniversary> &getAnniversary() const {
			return _anniversary;
		}
		
		void setGender(const shared_ptr<BelCardGender> &gender) {
			_gender = gender;
			addProperty(_gender);
		}
		const shared_ptr<BelCardGender> &getGender() const {
			return _gender;
		}
		
		void addNickname(const shared_ptr<BelCardNickname> &nickname) {
			_nicknames.push_back(nickname);
			addProperty(nickname);
		}
		const list<shared_ptr<BelCardNickname>> &getNicknames() const {
			return _nicknames;
		}
		
		void addPhoto(const shared_ptr<BelCardPhoto> &photo) {
			_photos.push_back(photo);
			addProperty(photo);
		}
		const list<shared_ptr<BelCardPhoto>> &getPhotos() const {
			return _photos;
		}
		
		void addAddress(const shared_ptr<BelCardAddress> &addr) {
			_addr.push_back(addr);
			addProperty(addr);
		}
		const list<shared_ptr<BelCardAddress>> &getAddresses() const {
			return _addr;
		}
		
		void addTel(const shared_ptr<BelCardTel> &tel) {
			_tel.push_back(tel);
			addProperty(tel);
		}
		const list<shared_ptr<BelCardTel>> &getTel() const {
			return _tel;
		}
		
		void addEmail(const shared_ptr<BelCardEmail> &email) {
			_emails.push_back(email);
			addProperty(email);
		}
		const list<shared_ptr<BelCardEmail>> &getEmails() const {
			return _emails;
		}
		
		void addImpp(const shared_ptr<BelCardImpp> &impp) {
			_impp.push_back(impp);
			addProperty(impp);
		}
		const list<shared_ptr<BelCardImpp>> &getImpp() const {
			return _impp;
		}
		
		void addLang(const shared_ptr<BelCardLang> &lang) {
			_langs.push_back(lang);
			addProperty(lang);
		}
		const list<shared_ptr<BelCardLang>> &getLangs() const {
			return _langs;
		}
		
		void addSource(const shared_ptr<BelCardSource> &source) {
			_sources.push_back(source);
			addProperty(source);
		}
		const list<shared_ptr<BelCardSource>> &getSource() const {
			return _sources;
		}
		
		void addXML(const shared_ptr<BelCardXML> &xml) {
			_xml.push_back(xml);
			addProperty(xml);
		}
		const list<shared_ptr<BelCardXML>> &getXML() const {
			return _xml;
		}
		
		void addProperty(const shared_ptr<BelCardProperty> &property) {
			_properties.push_back(property);
		}
		const list<shared_ptr<BelCardProperty>> &getProperties() const {
			return _properties;
		}
		
		friend ostream &operator<<(ostream &output, const BelCard &card) {
			output << "BEGIN:VCARD\r\nVERSION:4.0\r\n";
			for (auto it = card.getProperties().begin(); it != card.getProperties().end(); ++it) {
				output << (**it); 
			}
			output << "END:VCARD\r\n";
			return output;
		}
	};	
}

#endif