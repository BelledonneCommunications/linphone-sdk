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
			->setCollector("X-PROPERTY", make_sfn(&BelCard::_addExtendedProperty))
			->setCollector("SOURCE", make_sfn(&BelCard::_addSource))
			->setCollector("KIND", make_sfn(&BelCard::_setKind))
			->setCollector("XML", make_sfn(&BelCard::_addXML))
			->setCollector("FN", make_sfn(&BelCard::_setFullName))
			->setCollector("N", make_sfn(&BelCard::_setName))
			->setCollector("BDAY", make_sfn(&BelCard::_setBirthday))
			->setCollector("ANNIVERSARY", make_sfn(&BelCard::_setAnniversary))
			->setCollector("GENDER", make_sfn(&BelCard::_setGender))
			->setCollector("NICKNAME", make_sfn(&BelCard::_addNickname))
			->setCollector("PHOTO", make_sfn(&BelCard::_addPhoto))
			->setCollector("ADR", make_sfn(&BelCard::_addAddress))
			->setCollector("TEL", make_sfn(&BelCard::_addPhoneNumber))
			->setCollector("EMAIL", make_sfn(&BelCard::_addEmail))
			->setCollector("IMPP", make_sfn(&BelCard::_addImpp))
			->setCollector("LANG", make_sfn(&BelCard::_addLang))
			->setCollector("TZ", make_sfn(&BelCard::_addTimezone))
			->setCollector("GEO", make_sfn(&BelCard::_addGeo))
			->setCollector("TITLE", make_sfn(&BelCard::_addTitle))
			->setCollector("ROLE", make_sfn(&BelCard::_addRole))
			->setCollector("LOGO", make_sfn(&BelCard::_addLogo))
			->setCollector("ORG", make_sfn(&BelCard::_addOrganization))
			->setCollector("MEMBER", make_sfn(&BelCard::_addMember))
			->setCollector("RELATED", make_sfn(&BelCard::_addRelated))
			->setCollector("CATEGORIES", make_sfn(&BelCard::_addCategories))
			->setCollector("NOTE", make_sfn(&BelCard::_addNote))
			->setCollector("PRODID", make_sfn(&BelCard::_setProductId))
			->setCollector("REV", make_sfn(&BelCard::_setRevision))
			->setCollector("SOUND", make_sfn(&BelCard::_addSound))
			->setCollector("UID", make_sfn(&BelCard::_setUniqueId))
			->setCollector("CLIENTPIDMAP", make_sfn(&BelCard::_addClientProductIdMap))
			->setCollector("URL", make_sfn(&BelCard::_addURL))
			->setCollector("KEY", make_sfn(&BelCard::_addKey))
			->setCollector("FBURL", make_sfn(&BelCard::_addFBURL))
			->setCollector("CALADRURI", make_sfn(&BelCard::_addCALADRURI))
			->setCollector("CALURI", make_sfn(&BelCard::_addCALURI))
			->setCollector("BIRTHPLACE", make_sfn(&BelCard::_setBirthPlace))
			->setCollector("DEATHDATE", make_sfn(&BelCard::_setDeathDate))
			->setCollector("DEATHPLACE", make_sfn(&BelCard::_setDeathPlace));
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

void BelCard::_setKind(const shared_ptr<BelCardKind> &kind) {
	set(_kind, kind);
}
void BelCard::setKind(const shared_ptr<BelCardKind> &kind) {
	if (BelCardGeneric::isValid(kind)) {
		_setKind(kind);
	}
}
const shared_ptr<BelCardKind> &BelCard::getKind() const {
	return _kind;
}

void BelCard::_setFullName(const shared_ptr<BelCardFullName> &fn) {
	set(_fn, fn);
}
void BelCard::setFullName(const shared_ptr<BelCardFullName> &fn) {
	if (BelCardGeneric::isValid(fn)) {
		_setFullName(fn);
	}
}
const shared_ptr<BelCardFullName> &BelCard::getFullName() const {
	return _fn;
}

void BelCard::_setName(const shared_ptr<BelCardName> &n) {
	set(_n, n);
}
void BelCard::setName(const shared_ptr<BelCardName> &n) {
	if (BelCardGeneric::isValid(n)) {
		_setName(n);
	}
}
const shared_ptr<BelCardName> &BelCard::getName() const {
	return _n;
}

void BelCard::_setBirthday(const shared_ptr<BelCardBirthday> &bday) {
	set(_bday, bday);
}
void BelCard::setBirthday(const shared_ptr<BelCardBirthday> &bday) {
	if (BelCardGeneric::isValid(bday)) {
		_setBirthday(bday);
	}
}
const shared_ptr<BelCardBirthday> &BelCard::getBirthday() const {
	return _bday;
}

void BelCard::_setAnniversary(const shared_ptr<BelCardAnniversary> &anniversary) {
	set(_anniversary, anniversary);
}
void BelCard::setAnniversary(const shared_ptr<BelCardAnniversary> &anniversary) {
	if (BelCardGeneric::isValid(anniversary)) {
		_setAnniversary(anniversary);
	}
}
const shared_ptr<BelCardAnniversary> &BelCard::getAnniversary() const {
	return _anniversary;
}

void BelCard::_setGender(const shared_ptr<BelCardGender> &gender) {
	set(_gender, gender);
}
void BelCard::setGender(const shared_ptr<BelCardGender> &gender) {
	if (BelCardGeneric::isValid(gender)) {
		_setGender(gender);
	}
}
const shared_ptr<BelCardGender> &BelCard::getGender() const {
	return _gender;
}
		
void BelCard::_setProductId(const shared_ptr<BelCardProductId> &pid) {
	set(_pid, pid);
}
void BelCard::setProductId(const shared_ptr<BelCardProductId> &pid) {
	if (BelCardGeneric::isValid(pid)) {
		_setProductId(pid);
	}
}
const shared_ptr<BelCardProductId> &BelCard::getProductId() const {
	return _pid;
}

void BelCard::_setRevision(const shared_ptr<BelCardRevision> &rev) {
	set(_rev, rev);
}
void BelCard::setRevision(const shared_ptr<BelCardRevision> &rev) {
	if (BelCardGeneric::isValid(rev)) {
		_setRevision(rev);
	}
}
const shared_ptr<BelCardRevision> &BelCard::getRevision() const {
	return _rev;
}

void BelCard::_setUniqueId(const shared_ptr<BelCardUniqueId> &uid) {
	set(_uid, uid);
}
void BelCard::setUniqueId(const shared_ptr<BelCardUniqueId> &uid) {
	if (BelCardGeneric::isValid(uid)) {
		_setUniqueId(uid);
	}
}
const shared_ptr<BelCardUniqueId> &BelCard::getUniqueId() const {
	return _uid;
}

void BelCard::_setBirthPlace(const shared_ptr<BelCardBirthPlace> &place) {
	set(_bplace, place);
}
void BelCard::setBirthPlace(const shared_ptr<BelCardBirthPlace> &place) {
	if (BelCardGeneric::isValid(place)) {
		_setBirthPlace(place);
	}
}
const shared_ptr<BelCardBirthPlace> &BelCard::getBirthPlace() const {
	return _bplace;
}

void BelCard::_setDeathPlace(const shared_ptr<BelCardDeathPlace> &place) {
	set(_dplace, place);
}
void BelCard::setDeathPlace(const shared_ptr<BelCardDeathPlace> &place) {
	if (BelCardGeneric::isValid(place)) {
		_setDeathPlace(place);
	}
}
const shared_ptr<BelCardDeathPlace> &BelCard::getDeathPlace() const {
	return _dplace;
}

void BelCard::_setDeathDate(const shared_ptr<BelCardDeathDate> &date) {
	set(_ddate, date);
}
void BelCard::setDeathDate(const shared_ptr<BelCardDeathDate> &date) {
	if (BelCardGeneric::isValid(date)) {
		_setDeathDate(date);
	}
}
const shared_ptr<BelCardDeathDate> &BelCard::getDeathDate() const {
	return _ddate;
}

void BelCard::_addNickname(const shared_ptr<BelCardNickname> &nickname) {
	add(_nicknames, nickname);
}
void BelCard::addNickname(const shared_ptr<BelCardNickname> &nickname) {
	if (BelCardGeneric::isValid(nickname)) {
		_addNickname(nickname);
	}
}
void BelCard::removeNickname(const shared_ptr<BelCardNickname> &nickname) {
	remove(_nicknames, nickname);
}
const list<shared_ptr<BelCardNickname>> &BelCard::getNicknames() const {
	return _nicknames;
}

void BelCard::_addPhoto(const shared_ptr<BelCardPhoto> &photo) {
	add(_photos, photo);
}
void BelCard::addPhoto(const shared_ptr<BelCardPhoto> &photo) {
	if (BelCardGeneric::isValid(photo)) {
		_addPhoto(photo);
	}
}
void BelCard::removePhoto(const shared_ptr<BelCardPhoto> &photo) {
	remove(_photos, photo);
}
const list<shared_ptr<BelCardPhoto>> &BelCard::getPhotos() const {
	return _photos;
}

void BelCard::_addAddress(const shared_ptr<BelCardAddress> &addr) {
	add(_addr, addr);
}
void BelCard::addAddress(const shared_ptr<BelCardAddress> &addr) {
	if (BelCardGeneric::isValid(addr)) {
		_addAddress(addr);
	}
}
void BelCard::removeAddress(const shared_ptr<BelCardAddress> &addr) {
	remove(_addr, addr);
}
const list<shared_ptr<BelCardAddress>> &BelCard::getAddresses() const {
	return _addr;
}

void BelCard::_addPhoneNumber(const shared_ptr<BelCardPhoneNumber> &tel) {
	add(_tel, tel);
}
void BelCard::addPhoneNumber(const shared_ptr<BelCardPhoneNumber> &tel) {
	if (BelCardGeneric::isValid(tel)) {
		_addPhoneNumber(tel);
	}
}
void BelCard::removePhoneNumber(const shared_ptr<BelCardPhoneNumber> &tel) {
	remove(_tel, tel);
}
const list<shared_ptr<BelCardPhoneNumber>> &BelCard::getPhoneNumbers() const {
	return _tel;
}

void BelCard::_addEmail(const shared_ptr<BelCardEmail> &email) {
	add(_emails, email);
}
void BelCard::addEmail(const shared_ptr<BelCardEmail> &email) {
	if (BelCardGeneric::isValid(email)) {
		_addEmail(email);
	}
}
void BelCard::removeEmail(const shared_ptr<BelCardEmail> &email) {
	remove(_emails, email);
}
const list<shared_ptr<BelCardEmail>> &BelCard::getEmails() const {
	return _emails;
}

void BelCard::_addImpp(const shared_ptr<BelCardImpp> &impp) {
	add(_impp, impp);
}
void BelCard::addImpp(const shared_ptr<BelCardImpp> &impp) {
	if (BelCardGeneric::isValid(impp)) {
		_addImpp(impp);
	}
}
void BelCard::removeImpp(const shared_ptr<BelCardImpp> &impp) {
	remove(_impp, impp);
}
const list<shared_ptr<BelCardImpp>> &BelCard::getImpp() const {
	return _impp;
}

void BelCard::_addLang(const shared_ptr<BelCardLang> &lang) {
	add(_langs, lang);
}
void BelCard::addLang(const shared_ptr<BelCardLang> &lang) {
	if (BelCardGeneric::isValid(lang)) {
		_addLang(lang);
	}
}
void BelCard::removeLang(const shared_ptr<BelCardLang> &lang) {
	remove(_langs, lang);
}
const list<shared_ptr<BelCardLang>> &BelCard::getLangs() const {
	return _langs;
}

void BelCard::_addSource(const shared_ptr<BelCardSource> &source) {
	add(_sources, source);
}
void BelCard::addSource(const shared_ptr<BelCardSource> &source) {
	if (BelCardGeneric::isValid(source)) {
		_addSource(source);
	}
}
void BelCard::removeSource(const shared_ptr<BelCardSource> &source) {
	remove(_sources, source);
}
const list<shared_ptr<BelCardSource>> &BelCard::getSource() const {
	return _sources;
}

void BelCard::_addXML(const shared_ptr<BelCardXML> &xml) {
	add(_xml, xml);
}
void BelCard::addXML(const shared_ptr<BelCardXML> &xml) {
	if (BelCardGeneric::isValid(xml)) {
		_addXML(xml);
	}
}
void BelCard::removeXML(const shared_ptr<BelCardXML> &xml) {
	remove(_xml, xml);
}
const list<shared_ptr<BelCardXML>> &BelCard::getXML() const {
	return _xml;
}

void BelCard::_addTimezone(const shared_ptr<BelCardTimezone> &tz) {
	add(_timezones, tz);
}
void BelCard::addTimezone(const shared_ptr<BelCardTimezone> &tz) {
	if (BelCardGeneric::isValid(tz)) {
		_addTimezone(tz);
	}
}
void BelCard::removeTimezone(const shared_ptr<BelCardTimezone> &tz) {
	remove(_timezones, tz);
}
const list<shared_ptr<BelCardTimezone>> &BelCard::getTimezones() const {
	return _timezones;
}

void BelCard::_addGeo(const shared_ptr<BelCardGeo> &geo) {
	add(_geos, geo);
}
void BelCard::addGeo(const shared_ptr<BelCardGeo> &geo) {
	if (BelCardGeneric::isValid(geo)) {
		_addGeo(geo);
	}
}
void BelCard::removeGeo(const shared_ptr<BelCardGeo> &geo) {
	remove(_geos, geo);
}
const list<shared_ptr<BelCardGeo>> &BelCard::getGeos() const {
	return _geos;
}

void BelCard::_addTitle(const shared_ptr<BelCardTitle> &title) {
	add(_titles, title);
}
void BelCard::addTitle(const shared_ptr<BelCardTitle> &title) {
	if (BelCardGeneric::isValid(title)) {
		_addTitle(title);
	}
}
void BelCard::removeTitle(const shared_ptr<BelCardTitle> &title) {
	remove(_titles, title);
}
const list<shared_ptr<BelCardTitle>> &BelCard::getTitles() const {
	return _titles;
}

void BelCard::_addRole(const shared_ptr<BelCardRole> &role) {
	add(_roles, role);
}
void BelCard::addRole(const shared_ptr<BelCardRole> &role) {
	if (BelCardGeneric::isValid(role)) {
		_addRole(role);
	}
}
void BelCard::removeRole(const shared_ptr<BelCardRole> &role) {
	remove(_roles, role);
}
const list<shared_ptr<BelCardRole>> &BelCard::getRoles() const {
	return _roles;
}

void BelCard::_addLogo(const shared_ptr<BelCardLogo> &logo) {
	add(_logos, logo);
}
void BelCard::addLogo(const shared_ptr<BelCardLogo> &logo) {
	if (BelCardGeneric::isValid(logo)) {
		_addLogo(logo);
	}
}
void BelCard::removeLogo(const shared_ptr<BelCardLogo> &logo) {
	remove(_logos, logo);
}
const list<shared_ptr<BelCardLogo>> &BelCard::getLogos() const {
	return _logos;
}

void BelCard::_addOrganization(const shared_ptr<BelCardOrganization> &org) {
	add(_organizations, org);
}
void BelCard::addOrganization(const shared_ptr<BelCardOrganization> &org) {
	if (BelCardGeneric::isValid(org)) {
		_addOrganization(org);
	}
}
void BelCard::removeOrganization(const shared_ptr<BelCardOrganization> &org) {
	remove(_organizations, org);
}
const list<shared_ptr<BelCardOrganization>> &BelCard::getOrganizations() const {
	return _organizations;
}

void BelCard::_addMember(const shared_ptr<BelCardMember> &member) {
	add(_members, member);
}
void BelCard::addMember(const shared_ptr<BelCardMember> &member) {
	if (BelCardGeneric::isValid(member)) {
		_addMember(member);
	}
}
void BelCard::removeMember(const shared_ptr<BelCardMember> &member) {
	remove(_members, member);
}
const list<shared_ptr<BelCardMember>> &BelCard::getMembers() const {
	return _members;
}

void BelCard::_addRelated(const shared_ptr<BelCardRelated> &related) {
	add(_related, related);
}
void BelCard::addRelated(const shared_ptr<BelCardRelated> &related) {
	if (BelCardGeneric::isValid(related)) {
		_addRelated(related);
	}
}
void BelCard::removeRelated(const shared_ptr<BelCardRelated> &related) {
	remove(_related, related);
}
const list<shared_ptr<BelCardRelated>> &BelCard::getRelated() const {
	return _related;
}

void BelCard::_addCategories(const shared_ptr<BelCardCategories> &categories) {
	add(_categories, categories);
}
void BelCard::addCategories(const shared_ptr<BelCardCategories> &categories) {
	if (BelCardGeneric::isValid(categories)) {
		_addCategories(categories);
	}
}
void BelCard::removeCategories(const shared_ptr<BelCardCategories> &categories) {
	remove(_categories, categories);
}
const list<shared_ptr<BelCardCategories>> &BelCard::getCategories() const {
	return _categories;
}

void BelCard::_addNote(const shared_ptr<BelCardNote> &note) {
	add(_notes, note);
}
void BelCard::addNote(const shared_ptr<BelCardNote> &note) {
	if (BelCardGeneric::isValid(note)) {
		_addNote(note);
	}
}
void BelCard::removeNote(const shared_ptr<BelCardNote> &note) {
	remove(_notes, note);
}
const list<shared_ptr<BelCardNote>> &BelCard::getNotes() const {
	return _notes;
}

void BelCard::_addSound(const shared_ptr<BelCardSound> &sound) {
	add(_sounds, sound);
}
void BelCard::addSound(const shared_ptr<BelCardSound> &sound) {
	if (BelCardGeneric::isValid(sound)) {
		_addSound(sound);
	}
}
void BelCard::removeSound(const shared_ptr<BelCardSound> &sound) {
	remove(_sounds, sound);
}
const list<shared_ptr<BelCardSound>> &BelCard::getSounds() const {
	return _sounds;
}

void BelCard::_addClientProductIdMap(const shared_ptr<BelCardClientProductIdMap> &clientpidmap) {
	add(_clientpidmaps, clientpidmap);
}
void BelCard::addClientProductIdMap(const shared_ptr<BelCardClientProductIdMap> &clientpidmap) {
	if (BelCardGeneric::isValid(clientpidmap)) {
		_addClientProductIdMap(clientpidmap);
	}
}
void BelCard::removeClientProductIdMap(const shared_ptr<BelCardClientProductIdMap> &clientpidmap) {
	remove(_clientpidmaps, clientpidmap);
}
const list<shared_ptr<BelCardClientProductIdMap>> &BelCard::getClientProductIdMaps() const {
	return _clientpidmaps;
}

void BelCard::_addURL(const shared_ptr<BelCardURL> &url) {
	add(_urls, url);
}
void BelCard::addURL(const shared_ptr<BelCardURL> &url) {
	if (BelCardGeneric::isValid(url)) {
		_addURL(url);
	}
}
void BelCard::removeURL(const shared_ptr<BelCardURL> &url) {
	remove(_urls, url);
}
const list<shared_ptr<BelCardURL>> &BelCard::getURLs() const {
	return _urls;
}

void BelCard::_addKey(const shared_ptr<BelCardKey> &key) {
	add(_keys, key);
}
void BelCard::addKey(const shared_ptr<BelCardKey> &key) {
	if (BelCardGeneric::isValid(key)) {
		_addKey(key);
	}
}
void BelCard::removeKey(const shared_ptr<BelCardKey> &key) {
	remove(_keys, key);
}
const list<shared_ptr<BelCardKey>> &BelCard::getKeys() const {
	return _keys;
}

void BelCard::_addFBURL(const shared_ptr<BelCardFBURL> &fburl) {
	add(_fburls, fburl);
}
void BelCard::addFBURL(const shared_ptr<BelCardFBURL> &fburl) {
	if (BelCardGeneric::isValid(fburl)) {
		_addFBURL(fburl);
	}
}
void BelCard::removeFBURL(const shared_ptr<BelCardFBURL> &fburl) {
	remove(_fburls, fburl);
}
const list<shared_ptr<BelCardFBURL>> &BelCard::getFBURLs() const {
	return _fburls;
}

void BelCard::_addCALADRURI(const shared_ptr<BelCardCALADRURI> &caladruri) {
	add(_caladruris, caladruri);
}
void BelCard::addCALADRURI(const shared_ptr<BelCardCALADRURI> &caladruri) {
	if (BelCardGeneric::isValid(caladruri)) {
		_addCALADRURI(caladruri);
	}
}
void BelCard::removeCALADRURI(const shared_ptr<BelCardCALADRURI> &caladruri) {
	remove(_caladruris, caladruri);
}
const list<shared_ptr<BelCardCALADRURI>> &BelCard::getCALADRURIs() const {
	return _caladruris;
}

void BelCard::_addCALURI(const shared_ptr<BelCardCALURI> &caluri) {
	add(_caluris, caluri);
}
void BelCard::addCALURI(const shared_ptr<BelCardCALURI> &caluri) {
	if (BelCardGeneric::isValid(caluri)) {
		_addCALURI(caluri);
	}
}
void BelCard::removeCALURI(const shared_ptr<BelCardCALURI> &caluri) {
	remove(_caluris, caluri);
}
const list<shared_ptr<BelCardCALURI>> &BelCard::getCALURIs() const {
	return _caluris;
}

void BelCard::_addExtendedProperty(const shared_ptr<BelCardProperty> &property) {
	add(_extended_properties, property);
}
void BelCard::addExtendedProperty(const shared_ptr<BelCardProperty> &property) {
	if (BelCardGeneric::isValid(property)) {
		_addExtendedProperty(property);
	}
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