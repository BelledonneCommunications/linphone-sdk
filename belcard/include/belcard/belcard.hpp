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
		shared_ptr<BelCardProductId> _pid;
		shared_ptr<BelCardRevision> _rev;
		shared_ptr<BelCardUniqueId> _uid;
		list<shared_ptr<BelCardNickname>> _nicknames;
		list<shared_ptr<BelCardPhoto>> _photos;
		list<shared_ptr<BelCardAddress>> _addr;
		list<shared_ptr<BelCardTel>> _tel;
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
		
		void setProductId(const shared_ptr<BelCardProductId> &pid);
		const shared_ptr<BelCardProductId> &getProductId() const;
		
		void setRevision(const shared_ptr<BelCardRevision> &rev);
		const shared_ptr<BelCardRevision> &getRevision() const;
		
		void setUniqueId(const shared_ptr<BelCardUniqueId> &uid);
		const shared_ptr<BelCardUniqueId> &getUniqueId() const;
		
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
		
		string serialize() const;
	};	
}

#endif