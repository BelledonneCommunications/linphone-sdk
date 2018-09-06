/*
 Mediastreamer2 videotoolbox-utils.h
 Copyright (C) 2018 Belledonne Communications SARL

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <sstream>
#include <unordered_map>

#include "videotoolbox-utils.h"

using namespace std;

namespace mediastreamer {

static unordered_map<OSStatus, string> _errorMsg = {
	{ noErr                         , "no error"                 },
	{ kVTPropertyNotSupportedErr    , "property not supported"   },
	{ kVTVideoDecoderMalfunctionErr , "decoder malfunction"      },
	{ kVTInvalidSessionErr          , "invalid session"          },
	{ kVTParameterErr               , "parameter error"          },
	{ kCVReturnAllocationFailed     , "return allocation failed" },
	{ kVTVideoDecoderBadDataErr     , "decoding bad data"        }
};

std::string toString(::OSStatus status) {
	ostringstream message;
	unordered_map<OSStatus, string>::const_iterator it = _errorMsg.find(status);
	if (it != _errorMsg.cend()) {
		message << it->second;
	} else {
		message << "unknown error";
	}
	message << " [osstatus=" << int(status) << "]";
	return message.str();
}

} // namespace mediastreamer
