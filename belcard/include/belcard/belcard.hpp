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

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCard : public BelCardGeneric {
	private:
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
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCard();
		
		void setKind(const shared_ptr<BelCardKind> &kind);
		const shared_ptr<BelCardKind> &getKind() const;
		
		void setFullName(const shared_ptr<BelCardFullName> &fn);
		const shared_ptr<BelCardFullName> &getFullName() const;
		
		void setName(const shared_ptr<BelCardName> &n);
		const shared_ptr<BelCardName> &getName() const;
		
		void setBirthday(const shared_ptr<BelCardBirthday> &bday);
		const shared_ptr<BelCardBirthday> &getBirthday() const;
		
		void setAnniversary(const shared_ptr<BelCardAnniversary> &anniversary);
		const shared_ptr<BelCardAnniversary> &getAnniversary() const;
		
		void setGender(const shared_ptr<BelCardGender> &gender);
		const shared_ptr<BelCardGender> &getGender() const;
		
		void setProductId(const shared_ptr<BelCardProductId> &pid);
		const shared_ptr<BelCardProductId> &getProductId() const;
		
		void setRevision(const shared_ptr<BelCardRevision> &rev);
		const shared_ptr<BelCardRevision> &getRevision() const;
		
		void setUniqueId(const shared_ptr<BelCardUniqueId> &uid);
		const shared_ptr<BelCardUniqueId> &getUniqueId() const;
		
		void setBirthPlace(const shared_ptr<BelCardBirthPlace> &place);
		const shared_ptr<BelCardBirthPlace> &getBirthPlace() const;
		
		void setDeathPlace(const shared_ptr<BelCardDeathPlace> &place);
		const shared_ptr<BelCardDeathPlace> &getDeathPlace() const;
		
		void setDeathDate(const shared_ptr<BelCardDeathDate> &date);
		const shared_ptr<BelCardDeathDate> &getDeathDate() const;
		
		void addNickname(const shared_ptr<BelCardNickname> &nickname);
		const list<shared_ptr<BelCardNickname>> &getNicknames() const;
		
		void addPhoto(const shared_ptr<BelCardPhoto> &photo);
		const list<shared_ptr<BelCardPhoto>> &getPhotos() const;
		
		void addAddress(const shared_ptr<BelCardAddress> &addr);
		const list<shared_ptr<BelCardAddress>> &getAddresses() const;
		
		void addPhoneNumber(const shared_ptr<BelCardPhoneNumber> &tel);
		const list<shared_ptr<BelCardPhoneNumber>> &getPhoneNumbers() const;
		
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
	
		void addTimezone(const shared_ptr<BelCardTimezone> &tz);
		const list<shared_ptr<BelCardTimezone>> &getTimezones() const;
		
		void addGeo(const shared_ptr<BelCardGeo> &geo);
		const list<shared_ptr<BelCardGeo>> &getGeos() const;
		
		void addTitle(const shared_ptr<BelCardTitle> &title);
		const list<shared_ptr<BelCardTitle>> &getTitles() const;
		
		void addRole(const shared_ptr<BelCardRole> &role);
		const list<shared_ptr<BelCardRole>> &getRoles() const;
		
		void addLogo(const shared_ptr<BelCardLogo> &logo);
		const list<shared_ptr<BelCardLogo>> &getLogos() const;
		
		void addOrganization(const shared_ptr<BelCardOrganization> &org);
		const list<shared_ptr<BelCardOrganization>> &getOrganizations() const;
		
		void addMember(const shared_ptr<BelCardMember> &member);
		const list<shared_ptr<BelCardMember>> &getMembers() const;
		
		void addRelated(const shared_ptr<BelCardRelated> &related);
		const list<shared_ptr<BelCardRelated>> &getRelated() const;
		
		void addCategories(const shared_ptr<BelCardCategories> &categories);
		const list<shared_ptr<BelCardCategories>> &getCategories() const;
		
		void addNote(const shared_ptr<BelCardNote> &note);
		const list<shared_ptr<BelCardNote>> &getNotes() const;
		
		void addSound(const shared_ptr<BelCardSound> &sound);
		const list<shared_ptr<BelCardSound>> &getSounds() const;
		
		void addClientProductIdMap(const shared_ptr<BelCardClientProductIdMap> &clientpidmap);
		const list<shared_ptr<BelCardClientProductIdMap>> &getClientProductIdMaps() const;
		
		void addURL(const shared_ptr<BelCardURL> &url);
		const list<shared_ptr<BelCardURL>> &getURLs() const;
		
		void addKey(const shared_ptr<BelCardKey> &key);
		const list<shared_ptr<BelCardKey>> &getKeys() const;
		
		void addFBURL(const shared_ptr<BelCardFBURL> &fburl);
		const list<shared_ptr<BelCardFBURL>> &getFBURLs() const;
		
		void addCALADRURI(const shared_ptr<BelCardCALADRURI> &caladruri);
		const list<shared_ptr<BelCardCALADRURI>> &getCALADRURIs() const;
		
		void addCALURI(const shared_ptr<BelCardCALURI> &caluri);
		const list<shared_ptr<BelCardCALURI>> &getCALURIs() const;
		
		void addExtendedProperty(const shared_ptr<BelCardProperty> &property);
		const list<shared_ptr<BelCardProperty>> &getExtendedProperties() const;
		
		void addProperty(const shared_ptr<BelCardProperty> &property);
		const list<shared_ptr<BelCardProperty>> &getProperties() const;
		void removeProperty(const shared_ptr<BelCardProperty> &property);
		
		const string toFoldedString();
		const bool assertRFCCompliance();
		
		virtual void serialize(ostream &output) const;
	};
}

#endif