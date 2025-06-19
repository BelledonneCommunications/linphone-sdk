/*
 * Copyright (c) 2024-? Belledonne Communications SARL.
 *
 * This file is part of belle-sip.
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef channel_bank_h
#define channel_bank_h

#include "belle_sip_internal.h"

#ifdef __cplusplus

#include "belle-sip/object++.hh"
#include <map>
#include <string>

namespace bellesip {

template <typename _objT>
struct SmartCPointer {
	SmartCPointer(_objT *obj) {
		reset(obj);
	}
	SmartCPointer() = default;
	SmartCPointer(const SmartCPointer<_objT> &sp) {
		reset(sp.mPtr);
	}
	SmartCPointer(SmartCPointer<_objT> &&sp) {
		mPtr = sp.mPtr;
		sp.mPtr = nullptr;
	}
	void reset(_objT *obj = nullptr) {
		if (obj) belle_sip_object_ref(obj);
		if (mPtr) belle_sip_object_unref(mPtr);
		mPtr = obj;
	}
	~SmartCPointer() {
		reset();
	}
	_objT *mPtr = nullptr;
	bool operator==(const SmartCPointer<_objT> &other) {
		return mPtr == other.mPtr;
	}
	bool operator!=(const SmartCPointer<_objT> &other) {
		return mPtr != other.mPtr;
	}
	_objT *get() const {
		return mPtr;
	}
};

class ChannelBank : public HybridObject<belle_sip_channel_bank_t, ChannelBank> {
public:
	explicit ChannelBank() = default;
	ChannelBank(const ChannelBank &) = delete;
	~ChannelBank();
	belle_sip_channel_t *findChannel(const belle_sip_hop_t *hop, const struct addrinfo *addr) const;
	belle_sip_channel_t *findChannel(int ai_family, const belle_sip_hop_t *hop) const;
	belle_sip_channel_t *findChannel(const belle_sip_uri_t *local_uri) const;
	void addChannel(belle_sip_channel_t *channel);
	void removeChannel(belle_sip_channel_t *channel);
	// removes a channel if predicate returns != 0, returns the number of removed channels.
	size_t removeChannelIf(int (*func)(belle_sip_channel_t *, void *), void *user_data);
	size_t getCount() const;
	void clearAllChannels();
	void forEach(void (*func)(belle_sip_channel_t *, void *), void *user_data);
	std::string normalizeIdentifier(const char *identifier) const;

private:
	belle_sip_channel_t *findChannel(const std::list<SmartCPointer<belle_sip_channel_t>> &l,
	                                 const belle_sip_hop_t *hop,
	                                 const struct addrinfo *addr) const;
	std::map<std::string, std::list<SmartCPointer<belle_sip_channel_t>>> mChannelsById;
};

} // namespace bellesip

extern "C" {
#endif

belle_sip_channel_bank_t *belle_sip_channel_bank_new(void);

void belle_sip_channel_bank_add_channel(belle_sip_channel_bank_t *obj, belle_sip_channel_t *chan);

void belle_sip_channel_bank_remove_channel(belle_sip_channel_bank_t *obj, belle_sip_channel_t *chan);

size_t belle_sip_channel_bank_remove_if(belle_sip_channel_bank_t *obj,
                                        int (*func)(belle_sip_channel_t *, void *),
                                        void *user_data);

void belle_sip_channel_bank_for_each(belle_sip_channel_bank_t *obj,
                                     void (*func)(belle_sip_channel_t *, void *),
                                     void *user_data);

size_t belle_sip_channel_bank_get_count(belle_sip_channel_bank_t *obj);

void belle_sip_channel_bank_clear_all(belle_sip_channel_bank_t *obj);

belle_sip_channel_t *
belle_sip_channel_bank_find(belle_sip_channel_bank_t *obj, int ai_family, const belle_sip_hop_t *hop);

belle_sip_channel_t *belle_sip_channel_bank_find_by_addrinfo(belle_sip_channel_bank_t *obj,
                                                             const struct addrinfo *addr);
belle_sip_channel_t *belle_sip_channel_bank_find_by_local_uri(belle_sip_channel_bank_t *obj,
                                                              const belle_sip_uri_t *uri);

#ifdef __cplusplus
}
#endif

#endif
