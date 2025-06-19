/*
	lime_log.hpp
	@author Johan Pascal
	@copyright 	Copyright (C) 2018  Belledonne Communications SARL

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

#include <iomanip>
#include <sstream>
#include <memory>

#include "lime_log.hpp"
#include "lime/lime.hpp"

using namespace::std;

namespace lime {
	void hexStr(std::ostringstream &os, const uint8_t *data, size_t len, size_t digest) {
		os << std::dec<<"("<<len<<"B) "<<std::hex;
		if (len > 0) {;
			os << std::setw(2) << std::setfill('0') << (int)data[0];
			if (digest>0 && len>digest*2 ) {
				for( size_t i(1) ; i < digest; ++i ) {
					os << ", "<<std::setw(2) << std::setfill('0') << (int)data[i];
				}
				os << " .. "<<std::setw(2) << std::setfill('0') << (int)data[len-digest];
				for( size_t i(len -digest +1) ; i < len; ++i ) {
					os << ", "<<std::setw(2) << std::setfill('0') << (int)data[i];
				}
			} else {
				for( size_t i(1) ; i < len; ++i ) {
					os << ", "<<std::setw(2) << std::setfill('0') << (int)data[i];
				}
			}
		}
	}
	void EncryptionContext::dump(std::ostringstream &os, std::string indent) const {
		os<<std::endl<<indent<<"associatedData";
		hexStr(os, m_associatedData.data(), m_associatedData.size());
		os<<std::endl<<indent<<"recipients: ("<<m_recipients.size()<<")";
		for (const auto &recipient:m_recipients) {
			recipient.dump(os, "            ");
		}
		os<<std::endl<<indent<<"plainMessage";
		hexStr(os, m_plainMessage.data(),  m_plainMessage.size());
		os<<std::endl<<indent<<"cipherMessage: ";
		hexStr(os, m_cipherMessage.data(),  m_cipherMessage.size());
	}
	void RecipientData::dump(std::ostringstream &os, std::string indent) const {
		os<<std::endl<<indent<<"DeviceId: "<<deviceId<<std::endl;
		if (done) {
			os<<indent<<"done: true"<<std::endl;
		} else {
			os<<indent<<"done: false"<<std::endl;
		}
		os<<indent<<"peerStatus: "<<PeerDeviceStatus2String(peerStatus)<<std::endl;
		os<<indent<<"DRmessage: ";
		hexStr(os, DRmessage.data(),  DRmessage.size());
	}


} // namespace lime
