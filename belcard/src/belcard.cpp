#include "belcard/belcard.hpp"

using namespace::std;
using namespace::belr;
using namespace::belcard;

shared_ptr<BelCard> BelCard::create() {
	return make_shared<BelCard>();
}

void BelCard::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("vcard", make_fn(&BelCard::create))
			->setCollector("OTHER", make_sfn(&BelCard::addProperty))
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
			->setCollector("CALURI", make_sfn(&BelCard::addCALURI));
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
		
void BelCard::setProductId(const shared_ptr<BelCardProductId> &pid) {
	_pid = pid;
	addProperty(_pid);
}
const shared_ptr<BelCardProductId> &BelCard::getProductId() const {
	return _pid;
}

void BelCard::setRevision(const shared_ptr<BelCardRevision> &rev) {
	_rev = rev;
	addProperty(_rev);
}
const shared_ptr<BelCardRevision> &BelCard::getRevision() const {
	return _rev;
}

void BelCard::setUniqueId(const shared_ptr<BelCardUniqueId> &uid) {
	_uid = uid;
	addProperty(_uid);
}
const shared_ptr<BelCardUniqueId> &BelCard::getUniqueId() const {
	return _uid;
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

void BelCard::addTimezone(const shared_ptr<BelCardTimezone> &tz) {
	_timezones.push_back(tz);
	addProperty(tz);
}
const list<shared_ptr<BelCardTimezone>> &BelCard::getTimezones() const {
	return _timezones;
}

void BelCard::addGeo(const shared_ptr<BelCardGeo> &geo) {
	_geos.push_back(geo);
	addProperty(geo);
}
const list<shared_ptr<BelCardGeo>> &BelCard::getGeos() const {
	return _geos;
}

void BelCard::addTitle(const shared_ptr<BelCardTitle> &title) {
	_titles.push_back(title);
	addProperty(title);
}
const list<shared_ptr<BelCardTitle>> &BelCard::getTitles() const {
	return _titles;
}

void BelCard::addRole(const shared_ptr<BelCardRole> &role) {
	_roles.push_back(role);
	addProperty(role);
}
const list<shared_ptr<BelCardRole>> &BelCard::getRoles() const {
	return _roles;
}

void BelCard::addLogo(const shared_ptr<BelCardLogo> &logo) {
	_logos.push_back(logo);
	addProperty(logo);
}
const list<shared_ptr<BelCardLogo>> &BelCard::getLogos() const {
	return _logos;
}

void BelCard::addOrganization(const shared_ptr<BelCardOrganization> &org) {
	_organizations.push_back(org);
	addProperty(org);
}
const list<shared_ptr<BelCardOrganization>> &BelCard::getOrganizations() const {
	return _organizations;
}

void BelCard::addMember(const shared_ptr<BelCardMember> &member) {
	_members.push_back(member);
	addProperty(member);
}
const list<shared_ptr<BelCardMember>> &BelCard::getMembers() const {
	return _members;
}

void BelCard::addRelated(const shared_ptr<BelCardRelated> &related) {
	_related.push_back(related);
	addProperty(related);
}
const list<shared_ptr<BelCardRelated>> &BelCard::getRelated() const {
	return _related;
}

void BelCard::addCategories(const shared_ptr<BelCardCategories> &categories) {
	_categories.push_back(categories);
	addProperty(categories);
}
const list<shared_ptr<BelCardCategories>> &BelCard::getCategories() const {
	return _categories;
}

void BelCard::addNote(const shared_ptr<BelCardNote> &note) {
	_notes.push_back(note);
	addProperty(note);
}
const list<shared_ptr<BelCardNote>> &BelCard::getNotes() const {
	return _notes;
}

void BelCard::addSound(const shared_ptr<BelCardSound> &sound) {
	_sounds.push_back(sound);
	addProperty(sound);
}
const list<shared_ptr<BelCardSound>> &BelCard::getSounds() const {
	return _sounds;
}

void BelCard::addClientProductIdMap(const shared_ptr<BelCardClientProductIdMap> &clientpidmap) {
	_clientpidmaps.push_back(clientpidmap);
	addProperty(clientpidmap);
}
const list<shared_ptr<BelCardClientProductIdMap>> &BelCard::getClientProductIdMaps() const {
	return _clientpidmaps;
}

void BelCard::addURL(const shared_ptr<BelCardURL> &url) {
	_urls.push_back(url);
	addProperty(url);
}
const list<shared_ptr<BelCardURL>> &BelCard::getURLs() const {
	return _urls;
}

void BelCard::addKey(const shared_ptr<BelCardKey> &key) {
	_keys.push_back(key);
	addProperty(key);
}
const list<shared_ptr<BelCardKey>> &BelCard::getKeys() const {
	return _keys;
}

void BelCard::addFBURL(const shared_ptr<BelCardFBURL> &fburl) {
	_fburls.push_back(fburl);
	addProperty(fburl);
}
const list<shared_ptr<BelCardFBURL>> &BelCard::getFBURLs() const {
	return _fburls;
}

void BelCard::addCALADRURI(const shared_ptr<BelCardCALADRURI> &caladruri) {
	_caladruris.push_back(caladruri);
	addProperty(caladruri);
}
const list<shared_ptr<BelCardCALADRURI>> &BelCard::getCALADRURIs() const {
	return _caladruris;
}

void BelCard::addCALURI(const shared_ptr<BelCardCALURI> &caluri) {
	_caluris.push_back(caluri);
	addProperty(caluri);
}
const list<shared_ptr<BelCardCALURI>> &BelCard::getCALURIs() const {
	return _caluris;
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