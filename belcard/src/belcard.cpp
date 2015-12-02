#include "belcard/belcard.hpp"

using namespace::std;
using namespace::belr;
using namespace::belcard;

shared_ptr<BelCard> BelCard::create() {
	return make_shared<BelCard>();
}

void BelCard::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("vcard", make_fn(&BelCard::create))
			->setCollector("SOURCE", make_sfn(&BelCard::addSource))
			->setCollector("KIND", make_sfn(&BelCard::setKind))
			->setCollector("XML", make_sfn(&BelCard::addXML))
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
			->setCollector("LANG", make_sfn(&BelCard::addLang));
}

BelCard::BelCard() {
	
}

void BelCard::setKind(const shared_ptr<BelCardKind> &kind) {
	_kind = kind;
	addProperty(_kind);
}
const shared_ptr<BelCardKind> &BelCard::getKind() const {
	return _kind;
}

void BelCard::setFN(const shared_ptr<BelCardFN> &fn) {
	_fn = fn;
	addProperty(_fn);
}
const shared_ptr<BelCardFN> &BelCard::getFN() const {
	return _fn;
}

void BelCard::setN(const shared_ptr<BelCardN> &n) {
	_n = n;
	addProperty(_n);
}
const shared_ptr<BelCardN> &BelCard::getN() const {
	return _n;
}

void BelCard::setBirthday(const shared_ptr<BelCardBirthday> &bday) {
	_bday = bday;
	addProperty(_bday);
}
const shared_ptr<BelCardBirthday> &BelCard::getBirthday() const {
	return _bday;
}

void BelCard::setAnniversary(const shared_ptr<BelCardAnniversary> &anniversary) {
	_anniversary = anniversary;
	addProperty(_anniversary);
}
const shared_ptr<BelCardAnniversary> &BelCard::getAnniversary() const {
	return _anniversary;
}

void BelCard::setGender(const shared_ptr<BelCardGender> &gender) {
	_gender = gender;
	addProperty(_gender);
}
const shared_ptr<BelCardGender> &BelCard::getGender() const {
	return _gender;
}

void BelCard::addNickname(const shared_ptr<BelCardNickname> &nickname) {
	_nicknames.push_back(nickname);
	addProperty(nickname);
}
const list<shared_ptr<BelCardNickname>> &BelCard::getNicknames() const {
	return _nicknames;
}

void BelCard::addPhoto(const shared_ptr<BelCardPhoto> &photo) {
	_photos.push_back(photo);
	addProperty(photo);
}
const list<shared_ptr<BelCardPhoto>> &BelCard::getPhotos() const {
	return _photos;
}

void BelCard::addAddress(const shared_ptr<BelCardAddress> &addr) {
	_addr.push_back(addr);
	addProperty(addr);
}
const list<shared_ptr<BelCardAddress>> &BelCard::getAddresses() const {
	return _addr;
}

void BelCard::addTel(const shared_ptr<BelCardTel> &tel) {
	_tel.push_back(tel);
	addProperty(tel);
}
const list<shared_ptr<BelCardTel>> &BelCard::getTel() const {
	return _tel;
}

void BelCard::addEmail(const shared_ptr<BelCardEmail> &email) {
	_emails.push_back(email);
	addProperty(email);
}
const list<shared_ptr<BelCardEmail>> &BelCard::getEmails() const {
	return _emails;
}

void BelCard::addImpp(const shared_ptr<BelCardImpp> &impp) {
	_impp.push_back(impp);
	addProperty(impp);
}
const list<shared_ptr<BelCardImpp>> &BelCard::getImpp() const {
	return _impp;
}

void BelCard::addLang(const shared_ptr<BelCardLang> &lang) {
	_langs.push_back(lang);
	addProperty(lang);
}
const list<shared_ptr<BelCardLang>> &BelCard::getLangs() const {
	return _langs;
}

void BelCard::addSource(const shared_ptr<BelCardSource> &source) {
	_sources.push_back(source);
	addProperty(source);
}
const list<shared_ptr<BelCardSource>> &BelCard::getSource() const {
	return _sources;
}

void BelCard::addXML(const shared_ptr<BelCardXML> &xml) {
	_xml.push_back(xml);
	addProperty(xml);
}
const list<shared_ptr<BelCardXML>> &BelCard::getXML() const {
	return _xml;
}

void BelCard::addProperty(const shared_ptr<BelCardProperty> &property) {
	_properties.push_back(property);
}
const list<shared_ptr<BelCardProperty>> &BelCard::getProperties() const {
	return _properties;
}

string BelCard::serialize() const {
	stringstream output;
	
	output << "BEGIN:VCARD\r\nVERSION:4.0\r\n";
	for (auto it = getProperties().begin(); it != getProperties().end(); ++it) {
		output << (**it); 
	}
	output << "END:VCARD\r\n";
	
	return output.str();
}