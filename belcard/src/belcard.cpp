/*
	belcard.cpp
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

#include "belcard/belcard.hpp"
#include "belcard/belcard_utils.hpp"
#include <belr/parser-impl.cc>

using namespace::std;
using namespace::belr;
using namespace::belcard;

void BelCard::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("vcard", make_fn(BelCardGeneric::create<BelCard>))
			->setCollector("X-PROPERTY", make_sfn(&BelCard::addExtendedProperty))
			->setCollector("SOURCE", make_sfn(&BelCard::addSource))
			->setCollector("KIND", make_sfn(&BelCard::setKind))
			->setCollector("XML", make_sfn(&BelCard::addXML))
			->setCollector("FN", make_sfn(&BelCard::setFullName))
			->setCollector("N", make_sfn(&BelCard::setName))
			->setCollector("BDAY", make_sfn(&BelCard::setBirthday))
			->setCollector("ANNIVERSARY", make_sfn(&BelCard::setAnniversary))
			->setCollector("GENDER", make_sfn(&BelCard::setGender))
			->setCollector("NICKNAME", make_sfn(&BelCard::addNickname))
			->setCollector("PHOTO", make_sfn(&BelCard::addPhoto))
			->setCollector("ADR", make_sfn(&BelCard::addAddress))
			->setCollector("TEL", make_sfn(&BelCard::addPhoneNumber))
			->setCollector("EMAIL", make_sfn(&BelCard::addEmail))
			->setCollector("IMPP", make_sfn(&BelCard::addImpp))
			->setCollector("LANG", make_sfn(&BelCard::addLang))
			->setCollector("TZ", make_sfn(&BelCard::addTimezone))
			->setCollector("GEO", make_sfn(&BelCard::addGeo))
			->setCollector("TITLE", make_sfn(&BelCard::addTitle))
			->setCollector("ROLE", make_sfn(&BelCard::addRole))
			->setCollector("LOGO", make_sfn(&BelCard::addLogo))
			->setCollector("ORG", make_sfn(&BelCard::addOrganization))
			->setCollector("MEMBER", make_sfn(&BelCard::addMember))
			->setCollector("RELATED", make_sfn(&BelCard::addRelated))
			->setCollector("CATEGORIES", make_sfn(&BelCard::addCategories))
			->setCollector("NOTE", make_sfn(&BelCard::addNote))
			->setCollector("PRODID", make_sfn(&BelCard::setProductId))
			->setCollector("REV", make_sfn(&BelCard::setRevision))
			->setCollector("SOUND", make_sfn(&BelCard::addSound))
			->setCollector("UID", make_sfn(&BelCard::setUniqueId))
			->setCollector("CLIENTPIDMAP", make_sfn(&BelCard::addClientProductIdMap))
			->setCollector("URL", make_sfn(&BelCard::addURL))
			->setCollector("KEY", make_sfn(&BelCard::addKey))
			->setCollector("FBURL", make_sfn(&BelCard::addFBURL))
			->setCollector("CALADRURI", make_sfn(&BelCard::addCALADRURI))
			->setCollector("CALURI", make_sfn(&BelCard::addCALURI))
			->setCollector("BIRTHPLACE", make_sfn(&BelCard::setBirthPlace))
			->setCollector("DEATHDATE", make_sfn(&BelCard::setDeathDate))
			->setCollector("DEATHPLACE", make_sfn(&BelCard::setDeathPlace));
}

BelCard::BelCard() : BelCardGeneric() {
	
}

bool comparePropertiesUsingPrefParam(const shared_ptr<BelCardProperty>& prop1, const shared_ptr<BelCardProperty>& prop2) {
	shared_ptr<BelCardPrefParam> pref1 = prop1->getPrefParam();
	shared_ptr<BelCardPrefParam> pref2 = prop2->getPrefParam();
	if ((pref1 == nullptr) && (pref2 == nullptr)) return false;
	if (pref2 == nullptr) {
		return true;
	} else if (pref1 == nullptr) {
		return false;
	} else {
		return pref1->getValue() < pref2->getValue();
	}
}

template <typename T>
void BelCard::set(shared_ptr<T> &p, const shared_ptr<T> &property) {
	if (p) {
		removeProperty(p);
	}
	p = property;
	addProperty(property);
}

template <typename T>
void BelCard::add(list<shared_ptr<T>> &property_list, const shared_ptr<T> &property) {
	property_list.push_back(property);
	property_list.sort(comparePropertiesUsingPrefParam);
	addProperty(property);
}

template <typename T>
void BelCard::remove(list<shared_ptr<T>> &property_list, const shared_ptr<T> &property) {
	property_list.remove(property);
	removeProperty(property);
}

void BelCard::setKind(const shared_ptr<BelCardKind> &kind) {
	set(_kind, kind);
}
const shared_ptr<BelCardKind> &BelCard::getKind() const {
	return _kind;
}

void BelCard::setFullName(const shared_ptr<BelCardFullName> &fn) {
	set(_fn, fn);
}
const shared_ptr<BelCardFullName> &BelCard::getFullName() const {
	return _fn;
}

void BelCard::setName(const shared_ptr<BelCardName> &n) {
	set(_n, n);
}
const shared_ptr<BelCardName> &BelCard::getName() const {
	return _n;
}

void BelCard::setBirthday(const shared_ptr<BelCardBirthday> &bday) {
	set(_bday, bday);
}
const shared_ptr<BelCardBirthday> &BelCard::getBirthday() const {
	return _bday;
}

void BelCard::setAnniversary(const shared_ptr<BelCardAnniversary> &anniversary) {
	set(_anniversary, anniversary);
}
const shared_ptr<BelCardAnniversary> &BelCard::getAnniversary() const {
	return _anniversary;
}

void BelCard::setGender(const shared_ptr<BelCardGender> &gender) {
	set(_gender, gender);
}
const shared_ptr<BelCardGender> &BelCard::getGender() const {
	return _gender;
}
		
void BelCard::setProductId(const shared_ptr<BelCardProductId> &pid) {
	set(_pid, pid);
}
const shared_ptr<BelCardProductId> &BelCard::getProductId() const {
	return _pid;
}

void BelCard::setRevision(const shared_ptr<BelCardRevision> &rev) {
	set(_rev, rev);
}
const shared_ptr<BelCardRevision> &BelCard::getRevision() const {
	return _rev;
}

void BelCard::setUniqueId(const shared_ptr<BelCardUniqueId> &uid) {
	set(_uid, uid);
}
const shared_ptr<BelCardUniqueId> &BelCard::getUniqueId() const {
	return _uid;
}

void BelCard::setBirthPlace(const shared_ptr<BelCardBirthPlace> &place) {
	set(_bplace, place);
}
const shared_ptr<BelCardBirthPlace> &BelCard::getBirthPlace() const {
	return _bplace;
}

void BelCard::setDeathPlace(const shared_ptr<BelCardDeathPlace> &place) {
	set(_dplace, place);
}
const shared_ptr<BelCardDeathPlace> &BelCard::getDeathPlace() const {
	return _dplace;
}

void BelCard::setDeathDate(const shared_ptr<BelCardDeathDate> &date) {
	set(_ddate, date);
}
const shared_ptr<BelCardDeathDate> &BelCard::getDeathDate() const {
	return _ddate;
}

void BelCard::addNickname(const shared_ptr<BelCardNickname> &nickname) {
	add(_nicknames, nickname);
}
void BelCard::removeNickname(const shared_ptr<BelCardNickname> &nickname) {
	remove(_nicknames, nickname);
}
const list<shared_ptr<BelCardNickname>> &BelCard::getNicknames() const {
	return _nicknames;
}

void BelCard::addPhoto(const shared_ptr<BelCardPhoto> &photo) {
	add(_photos, photo);
}
void BelCard::removePhoto(const shared_ptr<BelCardPhoto> &photo) {
	remove(_photos, photo);
}
const list<shared_ptr<BelCardPhoto>> &BelCard::getPhotos() const {
	return _photos;
}

void BelCard::addAddress(const shared_ptr<BelCardAddress> &addr) {
	add(_addr, addr);
}
void BelCard::removeAddress(const shared_ptr<BelCardAddress> &addr) {
	remove(_addr, addr);
}
const list<shared_ptr<BelCardAddress>> &BelCard::getAddresses() const {
	return _addr;
}

void BelCard::addPhoneNumber(const shared_ptr<BelCardPhoneNumber> &tel) {
	add(_tel, tel);
}
void BelCard::removePhoneNumber(const shared_ptr<BelCardPhoneNumber> &tel) {
	remove(_tel, tel);
}
const list<shared_ptr<BelCardPhoneNumber>> &BelCard::getPhoneNumbers() const {
	return _tel;
}

void BelCard::addEmail(const shared_ptr<BelCardEmail> &email) {
	add(_emails, email);
}
void BelCard::removeEmail(const shared_ptr<BelCardEmail> &email) {
	remove(_emails, email);
}
const list<shared_ptr<BelCardEmail>> &BelCard::getEmails() const {
	return _emails;
}

void BelCard::addImpp(const shared_ptr<BelCardImpp> &impp) {
	add(_impp, impp);
}
void BelCard::removeImpp(const shared_ptr<BelCardImpp> &impp) {
	remove(_impp, impp);
}
const list<shared_ptr<BelCardImpp>> &BelCard::getImpp() const {
	return _impp;
}

void BelCard::addLang(const shared_ptr<BelCardLang> &lang) {
	add(_langs, lang);
}
void BelCard::removeLang(const shared_ptr<BelCardLang> &lang) {
	remove(_langs, lang);
}
const list<shared_ptr<BelCardLang>> &BelCard::getLangs() const {
	return _langs;
}

void BelCard::addSource(const shared_ptr<BelCardSource> &source) {
	add(_sources, source);
}
void BelCard::removeSource(const shared_ptr<BelCardSource> &source) {
	remove(_sources, source);
}
const list<shared_ptr<BelCardSource>> &BelCard::getSource() const {
	return _sources;
}

void BelCard::addXML(const shared_ptr<BelCardXML> &xml) {
	add(_xml, xml);
}
void BelCard::removeXML(const shared_ptr<BelCardXML> &xml) {
	remove(_xml, xml);
}
const list<shared_ptr<BelCardXML>> &BelCard::getXML() const {
	return _xml;
}

void BelCard::addTimezone(const shared_ptr<BelCardTimezone> &tz) {
	add(_timezones, tz);
}
void BelCard::removeTimezone(const shared_ptr<BelCardTimezone> &tz) {
	remove(_timezones, tz);
}
const list<shared_ptr<BelCardTimezone>> &BelCard::getTimezones() const {
	return _timezones;
}

void BelCard::addGeo(const shared_ptr<BelCardGeo> &geo) {
	add(_geos, geo);
}
void BelCard::removeGeo(const shared_ptr<BelCardGeo> &geo) {
	remove(_geos, geo);
}
const list<shared_ptr<BelCardGeo>> &BelCard::getGeos() const {
	return _geos;
}

void BelCard::addTitle(const shared_ptr<BelCardTitle> &title) {
	add(_titles, title);
}
void BelCard::removeTitle(const shared_ptr<BelCardTitle> &title) {
	remove(_titles, title);
}
const list<shared_ptr<BelCardTitle>> &BelCard::getTitles() const {
	return _titles;
}

void BelCard::addRole(const shared_ptr<BelCardRole> &role) {
	add(_roles, role);
}
void BelCard::removeRole(const shared_ptr<BelCardRole> &role) {
	remove(_roles, role);
}
const list<shared_ptr<BelCardRole>> &BelCard::getRoles() const {
	return _roles;
}

void BelCard::addLogo(const shared_ptr<BelCardLogo> &logo) {
	add(_logos, logo);
}
void BelCard::removeLogo(const shared_ptr<BelCardLogo> &logo) {
	remove(_logos, logo);
}
const list<shared_ptr<BelCardLogo>> &BelCard::getLogos() const {
	return _logos;
}

void BelCard::addOrganization(const shared_ptr<BelCardOrganization> &org) {
	add(_organizations, org);
}
void BelCard::removeOrganization(const shared_ptr<BelCardOrganization> &org) {
	remove(_organizations, org);
}
const list<shared_ptr<BelCardOrganization>> &BelCard::getOrganizations() const {
	return _organizations;
}

void BelCard::addMember(const shared_ptr<BelCardMember> &member) {
	add(_members, member);
}
void BelCard::removeMember(const shared_ptr<BelCardMember> &member) {
	remove(_members, member);
}
const list<shared_ptr<BelCardMember>> &BelCard::getMembers() const {
	return _members;
}

void BelCard::addRelated(const shared_ptr<BelCardRelated> &related) {
	add(_related, related);
}
void BelCard::removeRelated(const shared_ptr<BelCardRelated> &related) {
	remove(_related, related);
}
const list<shared_ptr<BelCardRelated>> &BelCard::getRelated() const {
	return _related;
}

void BelCard::addCategories(const shared_ptr<BelCardCategories> &categories) {
	add(_categories, categories);
}
void BelCard::removeCategories(const shared_ptr<BelCardCategories> &categories) {
	remove(_categories, categories);
}
const list<shared_ptr<BelCardCategories>> &BelCard::getCategories() const {
	return _categories;
}

void BelCard::addNote(const shared_ptr<BelCardNote> &note) {
	add(_notes, note);
}
void BelCard::removeNote(const shared_ptr<BelCardNote> &note) {
	remove(_notes, note);
}
const list<shared_ptr<BelCardNote>> &BelCard::getNotes() const {
	return _notes;
}

void BelCard::addSound(const shared_ptr<BelCardSound> &sound) {
	add(_sounds, sound);
}
void BelCard::removeSound(const shared_ptr<BelCardSound> &sound) {
	remove(_sounds, sound);
}
const list<shared_ptr<BelCardSound>> &BelCard::getSounds() const {
	return _sounds;
}

void BelCard::addClientProductIdMap(const shared_ptr<BelCardClientProductIdMap> &clientpidmap) {
	add(_clientpidmaps, clientpidmap);
}
void BelCard::removeClientProductIdMap(const shared_ptr<BelCardClientProductIdMap> &clientpidmap) {
	remove(_clientpidmaps, clientpidmap);
}
const list<shared_ptr<BelCardClientProductIdMap>> &BelCard::getClientProductIdMaps() const {
	return _clientpidmaps;
}

void BelCard::addURL(const shared_ptr<BelCardURL> &url) {
	add(_urls, url);
}
void BelCard::removeURL(const shared_ptr<BelCardURL> &url) {
	remove(_urls, url);
}
const list<shared_ptr<BelCardURL>> &BelCard::getURLs() const {
	return _urls;
}

void BelCard::addKey(const shared_ptr<BelCardKey> &key) {
	add(_keys, key);
}
void BelCard::removeKey(const shared_ptr<BelCardKey> &key) {
	remove(_keys, key);
}
const list<shared_ptr<BelCardKey>> &BelCard::getKeys() const {
	return _keys;
}

void BelCard::addFBURL(const shared_ptr<BelCardFBURL> &fburl) {
	add(_fburls, fburl);
}
void BelCard::removeFBURL(const shared_ptr<BelCardFBURL> &fburl) {
	remove(_fburls, fburl);
}
const list<shared_ptr<BelCardFBURL>> &BelCard::getFBURLs() const {
	return _fburls;
}

void BelCard::addCALADRURI(const shared_ptr<BelCardCALADRURI> &caladruri) {
	add(_caladruris, caladruri);
}
void BelCard::removeCALADRURI(const shared_ptr<BelCardCALADRURI> &caladruri) {
	remove(_caladruris, caladruri);
}
const list<shared_ptr<BelCardCALADRURI>> &BelCard::getCALADRURIs() const {
	return _caladruris;
}

void BelCard::addCALURI(const shared_ptr<BelCardCALURI> &caluri) {
	add(_caluris, caluri);
}
void BelCard::removeCALURI(const shared_ptr<BelCardCALURI> &caluri) {
	remove(_caluris, caluri);
}
const list<shared_ptr<BelCardCALURI>> &BelCard::getCALURIs() const {
	return _caluris;
}

void BelCard::addExtendedProperty(const shared_ptr<BelCardProperty> &property) {
	add(_extended_properties, property);
}
void BelCard::removeExtendedProperty(const shared_ptr<BelCardProperty> &property) {
	remove(_extended_properties, property);
}
const list<shared_ptr<BelCardProperty>> &BelCard::getExtendedProperties() const {
	return _extended_properties;
}

void BelCard::addProperty(const shared_ptr<BelCardProperty> &property) {
	_properties.push_back(property);
}
void BelCard::removeProperty(const shared_ptr<BelCardProperty> &property) {
	_properties.remove(property);
}
const list<shared_ptr<BelCardProperty>> &BelCard::getProperties() const {
	return _properties;
}

void BelCard::serialize(ostream& output) const {
	output << "BEGIN:VCARD\r\nVERSION:4.0\r\n";
	for (auto it = getProperties().begin(); it != getProperties().end(); ++it) {
		output << (**it); 
	}
	output << "END:VCARD\r\n";
}
		
string& BelCard::toFoldedString() {
	string temp = toString();
	_folded_string = belcard_fold(temp);
	return _folded_string;
}

bool BelCard::assertRFCCompliance() const {
	if (!_fn) {
		return false;
	}
	
	return true;
}

void BelCardList::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("vcard-list", make_fn(BelCardGeneric::create<BelCardList>))
			->setCollector("vcard", make_sfn(&BelCardList::addCard));
}

BelCardList::BelCardList() : BelCardGeneric() {
	
}

void BelCardList::addCard(const shared_ptr<BelCard> &vcard) {
	_vCards.push_back(vcard);
}

const list<shared_ptr<BelCard>> &BelCardList::getCards() const {
	return _vCards;
}

void BelCardList::serialize(ostream &output) const {
	for (auto it = getCards().begin(); it != getCards().end(); ++it) {
		output << (*it)->toFoldedString(); 
	}
}