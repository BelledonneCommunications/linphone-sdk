/*
 * address.cpp
 * Copyright (C) 2017  Belledonne Communications SARL
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY {} without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "linphone/utils/utils.h"
#include <sal/sal.h>

#include "address-p.h"
#include "c-wrapper/c-tools.h"
#include "logger/logger.h"

#include "address.h"

// =============================================================================

using namespace std;

LINPHONE_BEGIN_NAMESPACE

// -----------------------------------------------------------------------------

Address::Address (const string &address) : ClonableObject(*new AddressPrivate) {
	L_D(Address);
	if (!(d->internalAddress = sal_address_new(L_STRING_TO_C(address)))) {
		lWarning() << "Cannot create address, bad uri [" << address << "].";
		return;
	}

	AddressPrivate::AddressCache &cache = d->cache;
	cache.scheme = L_C_TO_STRING(sal_address_get_scheme(d->internalAddress));
	cache.displayName = L_C_TO_STRING(sal_address_get_display_name(d->internalAddress));
	cache.username = L_C_TO_STRING(sal_address_get_username(d->internalAddress));
	cache.domain = L_C_TO_STRING(sal_address_get_domain(d->internalAddress));
	cache.methodParam = L_C_TO_STRING(sal_address_get_method_param(d->internalAddress));
	cache.password = L_C_TO_STRING(sal_address_get_password(d->internalAddress));
}

Address::Address (const Address &src) : ClonableObject(*new AddressPrivate) {
	L_D(Address);
	SalAddress *salAddress = src.getPrivate()->internalAddress;
	if (salAddress) {
		d->internalAddress = sal_address_clone(salAddress);
		d->cache = src.getPrivate()->cache;
	}
}

Address::~Address () {
	L_D(Address);
	if (d->internalAddress)
		sal_address_destroy(d->internalAddress);
}

Address &Address::operator= (const Address &src) {
	L_D(Address);
	if (this != &src) {
		if (d->internalAddress)
			sal_address_destroy(d->internalAddress);
		SalAddress *salAddress = src.getPrivate()->internalAddress;
		d->internalAddress = salAddress ? sal_address_clone(salAddress) : nullptr;
		d->cache = src.getPrivate()->cache;
	}

	return *this;
}

Address::operator bool () const {
	L_D(const Address);
	return static_cast<bool>(d->internalAddress);
}

bool Address::operator== (const Address &address) const {
	return equal(address);
}

const string &Address::getScheme () const {
	L_D(const Address);
	return d->cache.scheme;
}

const string &Address::getDisplayName () const {
	L_D(const Address);
	return d->cache.displayName;
}

bool Address::setDisplayName (const string &displayName) {
	L_D(Address);

	if (!d->internalAddress)
		return false;

	sal_address_set_display_name(d->internalAddress, L_STRING_TO_C(displayName));
	d->cache.displayName = L_C_TO_STRING(sal_address_get_display_name(d->internalAddress));

	return true;
}

const string &Address::getUsername () const {
	L_D(const Address);
	return d->cache.username;
}

bool Address::setUsername (const string &username) {
	L_D(Address);

	if (!d->internalAddress)
		return false;

	sal_address_set_username(d->internalAddress, L_STRING_TO_C(username));
	d->cache.username = L_C_TO_STRING(sal_address_get_username(d->internalAddress));

	return true;
}

const string &Address::getDomain () const {
	L_D(const Address);
	return d->cache.domain;
}

bool Address::setDomain (const string &domain) {
	L_D(Address);

	if (!d->internalAddress)
		return false;

	sal_address_set_domain(d->internalAddress, L_STRING_TO_C(domain));
	d->cache.domain = L_C_TO_STRING(sal_address_get_domain(d->internalAddress));

	return true;
}

int Address::getPort () const {
	L_D(const Address);
	return d->internalAddress ? sal_address_get_port(d->internalAddress) : 0;
}

bool Address::setPort (int port) {
	L_D(const Address);

	if (!d->internalAddress)
		return false;

	sal_address_set_port(d->internalAddress, port);
	return true;
}

Transport Address::getTransport () const {
	L_D(const Address);
	return d->internalAddress ? static_cast<Transport>(sal_address_get_transport(d->internalAddress)) : Transport::Udp;
}

bool Address::setTransport (Transport transport) {
	L_D(const Address);

	if (!d->internalAddress)
		return false;

	sal_address_set_transport(d->internalAddress, static_cast<SalTransport>(transport));
	return true;
}

bool Address::getSecure () const {
	L_D(const Address);
	return d->internalAddress ? sal_address_is_secure(d->internalAddress) : false;
}

bool Address::setSecure (bool enabled) {
	L_D(const Address);

	if (!d->internalAddress)
		return false;

	sal_address_set_secure(d->internalAddress, enabled);
	return true;
}

bool Address::isSip () const {
	L_D(const Address);
	return d->internalAddress ? sal_address_is_sip(d->internalAddress) : false;
}

const string &Address::getMethodParam () const {
	L_D(const Address);
	return d->cache.methodParam;
}

bool Address::setMethodParam (const string &methodParam) {
	L_D(Address);

	if (!d->internalAddress)
		return false;

	sal_address_set_method_param(d->internalAddress, L_STRING_TO_C(methodParam));
	d->cache.methodParam = L_C_TO_STRING(sal_address_get_method_param(d->internalAddress));

	return true;
}

const string &Address::getPassword () const {
	L_D(const Address);
	return d->cache.password;
}

bool Address::setPassword (const string &password) {
	L_D(Address);

	if (!d->internalAddress)
		return false;

	sal_address_set_password(d->internalAddress, L_STRING_TO_C(password));
	d->cache.password = L_C_TO_STRING(sal_address_get_password(d->internalAddress));

	return true;
}

bool Address::clean () {
	L_D(const Address);

	if (!d->internalAddress)
		return false;

	sal_address_clean(d->internalAddress);
	return true;
}

string Address::asString () const {
	L_D(const Address);

	if (!d->internalAddress)
		return "";

	char *buf = sal_address_as_string(d->internalAddress);
	string out = buf;
	ms_free(buf);
	return out;
}

string Address::asStringUriOnly () const {
	L_D(const Address);

	if (!d->internalAddress)
		return "";

	char *buf = sal_address_as_string_uri_only(d->internalAddress);
	string out = buf;
	ms_free(buf);
	return out;
}

bool Address::equal (const Address &address) const {
	return asString() == address.asString();
}

bool Address::weakEqual (const Address &address) const {
	return getUsername() == address.getUsername() &&
				 getDomain() == address.getDomain() &&
				 getPort() == address.getPort();
}

const string &Address::getHeaderValue (const string &headerName) const {
	L_D(const Address);

	try {
		return d->cache.headers.at(headerName);
	} catch (const exception &) {
		const char *value = sal_address_get_header(d->internalAddress, L_STRING_TO_C(headerName));
		if (value) {
			d->cache.headers[headerName] = value;
			return d->cache.headers[headerName];
		}
	}

	return Utils::getEmptyConstRefObject<string>();
}

bool Address::setHeader (const string &headerName, const string &headerValue) {
	L_D(Address);

	if (!d->internalAddress)
		return false;

	sal_address_set_header(d->internalAddress, L_STRING_TO_C(headerName), L_STRING_TO_C(headerValue));
	d->cache.headers[headerName] = L_C_TO_STRING(sal_address_get_header(d->internalAddress, L_STRING_TO_C(headerName)));

	return true;
}

bool Address::hasParam (const string &paramName) const {
	L_D(const Address);
	return d->cache.params.find(paramName) != d->cache.params.cend();
}

const string &Address::getParamValue (const string &paramName) const {
	L_D(const Address);

	try {
		return d->cache.params.at(paramName);
	} catch (const exception &) {
		const char *value = sal_address_get_param(d->internalAddress, L_STRING_TO_C(paramName));
		if (value) {
			d->cache.params[paramName] = value;
			return d->cache.params[paramName];
		}
	}

	return Utils::getEmptyConstRefObject<string>();
}

bool Address::setParam (const string &paramName, const string &paramValue) {
	L_D(Address);

	if (!d->internalAddress)
		return false;

	sal_address_set_param(d->internalAddress, L_STRING_TO_C(paramName), L_STRING_TO_C(paramValue));
	d->cache.params[paramName] = L_C_TO_STRING(sal_address_get_param(d->internalAddress, L_STRING_TO_C(paramName)));

	return true;
}

bool Address::setParams (const string &params) {
	// TODO.
	return false;
}

bool Address::hasUriParam (const string &uriParamName) const {
	L_D(const Address);
	return d->cache.uriParams.find(uriParamName) != d->cache.uriParams.cend();
}

const string &Address::getUriParamValue (const string &uriParamName) const {
	L_D(const Address);

	try {
		return d->cache.uriParams.at(uriParamName);
	} catch (const exception &) {
		const char *value = sal_address_get_uri_param(d->internalAddress, L_STRING_TO_C(uriParamName));
		if (value) {
			d->cache.uriParams[uriParamName] = value;
			return d->cache.uriParams[uriParamName];
		}
	}

	return Utils::getEmptyConstRefObject<string>();
}

bool Address::setUriParam (const string &uriParamName, const string &uriParamValue) {
	L_D(Address);

	if (!d->internalAddress)
		return false;

	sal_address_set_uri_param(d->internalAddress, L_STRING_TO_C(uriParamName), L_STRING_TO_C(uriParamValue));
	d->cache.params[uriParamName] = L_C_TO_STRING(sal_address_get_uri_param(d->internalAddress, L_STRING_TO_C(uriParamName)));

	return true;
}

bool Address::setUriParams (const string &params) {
	// TODO.
	return false;
}

LINPHONE_END_NAMESPACE
