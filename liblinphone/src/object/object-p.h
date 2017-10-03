/*
 * object-p.h
 * Copyright (C) 2017  Belledonne Communications SARL
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _OBJECT_P_H_
#define _OBJECT_P_H_

#include <memory>
#include <unordered_map>

#include "variant/variant.h"

// =============================================================================

LINPHONE_BEGIN_NAMESPACE

class ObjectPrivate {
	friend class ObjectFactory;

public:
	virtual ~ObjectPrivate () = default;

protected:
	Object *mPublic;

private:
	std::unordered_map<std::string, Variant> properties;
	std::weak_ptr<Object> weak;

	L_DECLARE_PUBLIC(Object);
};

LINPHONE_END_NAMESPACE

#endif // ifndef _OBJECT_P_H_
