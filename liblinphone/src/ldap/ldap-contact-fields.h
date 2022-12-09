/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of Liblinphone 
 * (see https://gitlab.linphone.org/BC/public/liblinphone).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LINPHONE_LDAP_CONTACT_FIELDS_H_
#define LINPHONE_LDAP_CONTACT_FIELDS_H_

#include "linphone/types.h"
#include <vector>
#include <map>

LINPHONE_BEGIN_NAMESPACE

class LdapContactFields {
public:
	LdapContactFields();

	/**
	 * Regroup findings and keep the best choices.
	 * 
	 * A map of sip addresses to ensure having uniques. Value is the number associated to the address
	 * A list can be all mobile numbers
	 */
	std::pair< std::string, int> mName;
	std::map<std::string, std::string> mSip;
};

LINPHONE_END_NAMESPACE

#endif /* LINPHONE_LDAP_CONTACT_FIELDS_H_ */
