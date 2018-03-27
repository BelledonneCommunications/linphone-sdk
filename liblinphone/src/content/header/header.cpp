/*
 * header.cpp
 * Copyright (C) 2010-2018 Belledonne Communications SARL
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "linphone/utils/utils.h"
#include "linphone/utils/algorithm.h"

#include "header-p.h"
#include "header-param.h"

// =============================================================================

using namespace std;

LINPHONE_BEGIN_NAMESPACE

// -----------------------------------------------------------------------------

Header::Header(HeaderPrivate &p) : ClonableObject(p) {

}

Header::Header (const string &name, const string &value) : ClonableObject(*new HeaderPrivate) {
	setName(name);
	setValue(value);
}

Header::Header (const string &name, const string &value, const list<HeaderParam> &params) : Header(name, value) {
	addParameters(params);
}

Header::Header (const Header &other) : Header(other.getName(), other.getValue(), other.getParameters()) {}

Header &Header::operator= (const Header &other) {
	if (this != &other) {
		setName(other.getName());
		setValue(other.getValue());
		cleanParameters();
		addParameters(other.getParameters());
	}

	return *this;
}

bool Header::operator== (const Header &other) const {
	return getName() == other.getName() &&
		getValue() == other.getValue();
}

bool Header::operator!= (const Header &other) const {
	return !(*this == other);
}

void Header::setName (const string &name) {
	L_D();
	d->name = name;
}

string Header::getName () const {
	L_D();
	return d->name;
}

void Header::setValue (const string &value) {
	L_D();
	d->value = value;
}

string Header::getValue () const {
	L_D();
	return d->value;
}

void Header::cleanParameters () {
	L_D();
	d->parameters.clear();
}

const std::list<HeaderParam> &Header::getParameters () const {
	L_D();
	return d->parameters;
}

void Header::addParameter (const std::string &paramName, const std::string &paramValue) {
	addParameter(HeaderParam(paramName, paramValue));
}

void Header::addParameter (const HeaderParam &param) {
	L_D();
	removeParameter(param);
	d->parameters.push_back(param);
}

void Header::addParameters(const std::list<HeaderParam> &params) {
	for (auto it = std::begin(params); it!=std::end(params); ++it) {
		HeaderParam param = *it;
    	addParameter(param.getName(), param.getValue());
	}
}

void Header::removeParameter (const std::string &paramName) {
	L_D();
	auto it = findParameter(paramName);
	if (it != d->parameters.cend())
		d->parameters.remove(*it);
}

void Header::removeParameter (const HeaderParam &param) {
	removeParameter(param.getName());
}

std::list<HeaderParam>::const_iterator Header::findParameter (const std::string &paramName) const {
	L_D();
	return findIf(d->parameters, [&paramName](const HeaderParam &param) {
		return param.getName() == paramName;
	});
}

const HeaderParam &Header::getParameter (const std::string &paramName) const {
	L_D();
	std::list<HeaderParam>::const_iterator it = findParameter(paramName);
	if (it != d->parameters.cend()) {
		return *it;
	}
	return Utils::getEmptyConstRefObject<HeaderParam>();
}

string Header::asString () const {
	string asString = getName() + ":" + getValue();
	for (const auto &param : getParameters()) {
		asString += param.asString();
	}
	return asString;
}

ostream &operator<<(ostream& stream, const Header& header) {
	stream << header.asString();
	return stream;
}

LINPHONE_END_NAMESPACE