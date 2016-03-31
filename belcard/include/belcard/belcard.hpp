/*
	belcard.hpp
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

#ifndef belcard_hpp
#define belcard_hpp

#include "belcard_generic.hpp"
#include "belcard_params.hpp"
#include "belcard_property.hpp"
#include "belcard_general.hpp"
#include "belcard_identification.hpp"
#include "belcard_addressing.hpp"
#include "belcard_communication.hpp"
#include "belcard_geographical.hpp"
#include "belcard_organizational.hpp"
#include "belcard_explanatory.hpp"
#include "belcard_security.hpp"
#include "belcard_calendar.hpp"
#include "belcard_rfc6474.hpp"

#include <string>
#include <list>

#ifndef BELCARD_PUBLIC
#if defined(_MSC_VER)
#define BELCARD_PUBLIC	__declspec(dllexport)
#else
#define BELCARD_PUBLIC
#endif
#endif

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCard : public BelCardGeneric {
	private:
		std::string _folded_string;
		
		shared_ptr<BelCardKind> _kind;
		shared_ptr<BelCardFullName> _fn;
		shared_ptr<BelCardName> _n;
		shared_ptr<BelCardBirthday> _bday;
		shared_ptr<BelCardAnniversary> _anniversary;
		shared_ptr<BelCardGender> _gender;
		shared_ptr<BelCardProductId> _pid;
		shared_ptr<BelCardRevision> _rev;
		shared_ptr<BelCardUniqueId> _uid;
		shared_ptr<BelCardBirthPlace> _bplace;
		shared_ptr<BelCardDeathPlace> _dplace;
		shared_ptr<BelCardDeathDate> _ddate;
		list<shared_ptr<BelCardNickname>> _nicknames;
		list<shared_ptr<BelCardPhoto>> _photos;
		list<shared_ptr<BelCardAddress>> _addr;
		list<shared_ptr<BelCardPhoneNumber>> _tel;
		list<shared_ptr<BelCardEmail>> _emails;
		list<shared_ptr<BelCardImpp>> _impp;
		list<shared_ptr<BelCardLang>> _langs;
		list<shared_ptr<BelCardSource>> _sources;
		list<shared_ptr<BelCardXML>> _xml;
		list<shared_ptr<BelCardTimezone>> _timezones;
		list<shared_ptr<BelCardGeo>> _geos;
		list<shared_ptr<BelCardTitle>> _titles;
		list<shared_ptr<BelCardRole>> _roles;
		list<shared_ptr<BelCardLogo>> _logos;
		list<shared_ptr<BelCardOrganization>> _organizations;
		list<shared_ptr<BelCardMember>> _members;
		list<shared_ptr<BelCardRelated>> _related;
		list<shared_ptr<BelCardCategories>> _categories;
		list<shared_ptr<BelCardNote>> _notes;
		list<shared_ptr<BelCardSound>> _sounds;
		list<shared_ptr<BelCardClientProductIdMap>> _clientpidmaps;
		list<shared_ptr<BelCardURL>> _urls;
		list<shared_ptr<BelCardKey>> _keys;
		list<shared_ptr<BelCardFBURL>> _fburls;
		list<shared_ptr<BelCardCALADRURI>> _caladruris;
		list<shared_ptr<BelCardCALURI>> _caluris;
		list<shared_ptr<BelCardProperty>> _extended_properties;
		list<shared_ptr<BelCardProperty>> _properties;
		
	public:
		BELCARD_PUBLIC static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BELCARD_PUBLIC BelCard();
		
		BELCARD_PUBLIC void setKind(const shared_ptr<BelCardKind> &kind);
		BELCARD_PUBLIC const shared_ptr<BelCardKind> &getKind() const;
		
		BELCARD_PUBLIC void setFullName(const shared_ptr<BelCardFullName> &fn);
		BELCARD_PUBLIC const shared_ptr<BelCardFullName> &getFullName() const;
		
		BELCARD_PUBLIC void setName(const shared_ptr<BelCardName> &n);
		BELCARD_PUBLIC const shared_ptr<BelCardName> &getName() const;
		
		BELCARD_PUBLIC void setBirthday(const shared_ptr<BelCardBirthday> &bday);
		BELCARD_PUBLIC const shared_ptr<BelCardBirthday> &getBirthday() const;
		
		BELCARD_PUBLIC void setAnniversary(const shared_ptr<BelCardAnniversary> &anniversary);
		BELCARD_PUBLIC const shared_ptr<BelCardAnniversary> &getAnniversary() const;
		
		BELCARD_PUBLIC void setGender(const shared_ptr<BelCardGender> &gender);
		BELCARD_PUBLIC const shared_ptr<BelCardGender> &getGender() const;
		
		BELCARD_PUBLIC void setProductId(const shared_ptr<BelCardProductId> &pid);
		BELCARD_PUBLIC const shared_ptr<BelCardProductId> &getProductId() const;
		
		BELCARD_PUBLIC void setRevision(const shared_ptr<BelCardRevision> &rev);
		BELCARD_PUBLIC const shared_ptr<BelCardRevision> &getRevision() const;
		
		BELCARD_PUBLIC void setUniqueId(const shared_ptr<BelCardUniqueId> &uid);
		BELCARD_PUBLIC const shared_ptr<BelCardUniqueId> &getUniqueId() const;
		
		BELCARD_PUBLIC void setBirthPlace(const shared_ptr<BelCardBirthPlace> &place);
		BELCARD_PUBLIC const shared_ptr<BelCardBirthPlace> &getBirthPlace() const;
		
		BELCARD_PUBLIC void setDeathPlace(const shared_ptr<BelCardDeathPlace> &place);
		BELCARD_PUBLIC const shared_ptr<BelCardDeathPlace> &getDeathPlace() const;
		
		BELCARD_PUBLIC void setDeathDate(const shared_ptr<BelCardDeathDate> &date);
		BELCARD_PUBLIC const shared_ptr<BelCardDeathDate> &getDeathDate() const;
		
		BELCARD_PUBLIC void addNickname(const shared_ptr<BelCardNickname> &nickname);
		BELCARD_PUBLIC void removeNickname(const shared_ptr<BelCardNickname> &nickname);
		BELCARD_PUBLIC const list<shared_ptr<BelCardNickname>> &getNicknames() const;
		
		BELCARD_PUBLIC void addPhoto(const shared_ptr<BelCardPhoto> &photo);
		BELCARD_PUBLIC void removePhoto(const shared_ptr<BelCardPhoto> &photo);
		BELCARD_PUBLIC const list<shared_ptr<BelCardPhoto>> &getPhotos() const;
		
		BELCARD_PUBLIC void addAddress(const shared_ptr<BelCardAddress> &addr);
		BELCARD_PUBLIC void removeAddress(const shared_ptr<BelCardAddress> &addr);
		BELCARD_PUBLIC const list<shared_ptr<BelCardAddress>> &getAddresses() const;
		
		BELCARD_PUBLIC void addPhoneNumber(const shared_ptr<BelCardPhoneNumber> &tel);
		BELCARD_PUBLIC void removePhoneNumber(const shared_ptr<BelCardPhoneNumber> &tel);
		BELCARD_PUBLIC const list<shared_ptr<BelCardPhoneNumber>> &getPhoneNumbers() const;
		
		BELCARD_PUBLIC void addEmail(const shared_ptr<BelCardEmail> &email);
		BELCARD_PUBLIC void removeEmail(const shared_ptr<BelCardEmail> &email);
		BELCARD_PUBLIC const list<shared_ptr<BelCardEmail>> &getEmails() const;
		
		BELCARD_PUBLIC void addImpp(const shared_ptr<BelCardImpp> &impp);
		BELCARD_PUBLIC void removeImpp(const shared_ptr<BelCardImpp> &impp);
		BELCARD_PUBLIC const list<shared_ptr<BelCardImpp>> &getImpp() const;
		
		BELCARD_PUBLIC void addLang(const shared_ptr<BelCardLang> &lang);
		BELCARD_PUBLIC void removeLang(const shared_ptr<BelCardLang> &lang);
		BELCARD_PUBLIC const list<shared_ptr<BelCardLang>> &getLangs() const;
		
		BELCARD_PUBLIC void addSource(const shared_ptr<BelCardSource> &source);
		BELCARD_PUBLIC void removeSource(const shared_ptr<BelCardSource> &source);
		BELCARD_PUBLIC const list<shared_ptr<BelCardSource>> &getSource() const;
		
		BELCARD_PUBLIC void addXML(const shared_ptr<BelCardXML> &xml);
		BELCARD_PUBLIC void removeXML(const shared_ptr<BelCardXML> &xml);
		BELCARD_PUBLIC const list<shared_ptr<BelCardXML>> &getXML() const;
	
		BELCARD_PUBLIC void addTimezone(const shared_ptr<BelCardTimezone> &tz);
		BELCARD_PUBLIC void removeTimezone(const shared_ptr<BelCardTimezone> &tz);
		BELCARD_PUBLIC const list<shared_ptr<BelCardTimezone>> &getTimezones() const;
		
		BELCARD_PUBLIC void addGeo(const shared_ptr<BelCardGeo> &geo);
		BELCARD_PUBLIC void removeGeo(const shared_ptr<BelCardGeo> &geo);
		BELCARD_PUBLIC const list<shared_ptr<BelCardGeo>> &getGeos() const;
		
		BELCARD_PUBLIC void addTitle(const shared_ptr<BelCardTitle> &title);
		BELCARD_PUBLIC void removeTitle(const shared_ptr<BelCardTitle> &title);
		BELCARD_PUBLIC const list<shared_ptr<BelCardTitle>> &getTitles() const;
		
		BELCARD_PUBLIC void addRole(const shared_ptr<BelCardRole> &role);
		BELCARD_PUBLIC void removeRole(const shared_ptr<BelCardRole> &role);
		BELCARD_PUBLIC const list<shared_ptr<BelCardRole>> &getRoles() const;
		
		BELCARD_PUBLIC void addLogo(const shared_ptr<BelCardLogo> &logo);
		BELCARD_PUBLIC void removeLogo(const shared_ptr<BelCardLogo> &logo);
		BELCARD_PUBLIC const list<shared_ptr<BelCardLogo>> &getLogos() const;
		
		BELCARD_PUBLIC void addOrganization(const shared_ptr<BelCardOrganization> &org);
		BELCARD_PUBLIC void removeOrganization(const shared_ptr<BelCardOrganization> &org);
		BELCARD_PUBLIC const list<shared_ptr<BelCardOrganization>> &getOrganizations() const;
		
		BELCARD_PUBLIC void addMember(const shared_ptr<BelCardMember> &member);
		BELCARD_PUBLIC void removeMember(const shared_ptr<BelCardMember> &member);
		BELCARD_PUBLIC const list<shared_ptr<BelCardMember>> &getMembers() const;
		
		BELCARD_PUBLIC void addRelated(const shared_ptr<BelCardRelated> &related);
		BELCARD_PUBLIC void removeRelated(const shared_ptr<BelCardRelated> &related);
		BELCARD_PUBLIC const list<shared_ptr<BelCardRelated>> &getRelated() const;
		
		BELCARD_PUBLIC void addCategories(const shared_ptr<BelCardCategories> &categories);
		BELCARD_PUBLIC void removeCategories(const shared_ptr<BelCardCategories> &categories);
		BELCARD_PUBLIC const list<shared_ptr<BelCardCategories>> &getCategories() const;
		
		BELCARD_PUBLIC void addNote(const shared_ptr<BelCardNote> &note);
		BELCARD_PUBLIC void removeNote(const shared_ptr<BelCardNote> &note);
		BELCARD_PUBLIC const list<shared_ptr<BelCardNote>> &getNotes() const;
		
		BELCARD_PUBLIC void addSound(const shared_ptr<BelCardSound> &sound);
		BELCARD_PUBLIC void removeSound(const shared_ptr<BelCardSound> &sound);
		BELCARD_PUBLIC const list<shared_ptr<BelCardSound>> &getSounds() const;
		
		BELCARD_PUBLIC void addClientProductIdMap(const shared_ptr<BelCardClientProductIdMap> &clientpidmap);
		BELCARD_PUBLIC void removeClientProductIdMap(const shared_ptr<BelCardClientProductIdMap> &clientpidmap);
		BELCARD_PUBLIC const list<shared_ptr<BelCardClientProductIdMap>> &getClientProductIdMaps() const;
		
		BELCARD_PUBLIC void addURL(const shared_ptr<BelCardURL> &url);
		BELCARD_PUBLIC void removeURL(const shared_ptr<BelCardURL> &url);
		BELCARD_PUBLIC const list<shared_ptr<BelCardURL>> &getURLs() const;
		
		BELCARD_PUBLIC void addKey(const shared_ptr<BelCardKey> &key);
		BELCARD_PUBLIC void removeKey(const shared_ptr<BelCardKey> &key);
		BELCARD_PUBLIC const list<shared_ptr<BelCardKey>> &getKeys() const;
		
		BELCARD_PUBLIC void addFBURL(const shared_ptr<BelCardFBURL> &fburl);
		BELCARD_PUBLIC void removeFBURL(const shared_ptr<BelCardFBURL> &fburl);
		BELCARD_PUBLIC const list<shared_ptr<BelCardFBURL>> &getFBURLs() const;
		
		BELCARD_PUBLIC void addCALADRURI(const shared_ptr<BelCardCALADRURI> &caladruri);
		BELCARD_PUBLIC void removeCALADRURI(const shared_ptr<BelCardCALADRURI> &caladruri);
		BELCARD_PUBLIC const list<shared_ptr<BelCardCALADRURI>> &getCALADRURIs() const;
		
		BELCARD_PUBLIC void addCALURI(const shared_ptr<BelCardCALURI> &caluri);
		BELCARD_PUBLIC void removeCALURI(const shared_ptr<BelCardCALURI> &caluri);
		BELCARD_PUBLIC const list<shared_ptr<BelCardCALURI>> &getCALURIs() const;
		
		BELCARD_PUBLIC void addExtendedProperty(const shared_ptr<BelCardProperty> &property);
		BELCARD_PUBLIC void removeExtendedProperty(const shared_ptr<BelCardProperty> &property);
		BELCARD_PUBLIC const list<shared_ptr<BelCardProperty>> &getExtendedProperties() const;
		
		BELCARD_PUBLIC void addProperty(const shared_ptr<BelCardProperty> &property);
		BELCARD_PUBLIC void removeProperty(const shared_ptr<BelCardProperty> &property);
		BELCARD_PUBLIC const list<shared_ptr<BelCardProperty>> &getProperties() const;
		
		BELCARD_PUBLIC string& toFoldedString();
		BELCARD_PUBLIC bool assertRFCCompliance() const;
		
		BELCARD_PUBLIC virtual void serialize(ostream &output) const;
	};
	
	class BelCardList : public BelCardGeneric {
	private:
		list<shared_ptr<BelCard>> _vCards;
		
	public:
		BELCARD_PUBLIC static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BELCARD_PUBLIC BelCardList();
		
		BELCARD_PUBLIC void addCard(const shared_ptr<BelCard> &vcard);
		BELCARD_PUBLIC const list<shared_ptr<BelCard>> &getCards() const;
		
		BELCARD_PUBLIC void serialize(ostream &output) const;
	};
}

#endif
