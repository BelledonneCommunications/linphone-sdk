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

#include <algorithm>

#include "channel_bank.hh"

namespace bellesip {

void ChannelBank::addChannel(belle_sip_channel_t *channel) {
	auto &l = mChannelsById[normalizeIdentifier(belle_sip_channel_get_bank_identifier(channel))];
	/* The channel with names must be treated with higher priority by the get_channel() method so queued on front.
	 * This is to prevent the UDP listening point to dispatch incoming messages to channels that were created by inbound
	 * connection where name cannot be determined. When this arrives, there can be 2 channels for the same destination
	 * IP and strange problems can occur where requests are sent through name qualified channel and response received
	 * through name unqualified channel.
	 */
	if (channel->has_name) l.push_front(channel);
	else l.push_back(channel);
}

void ChannelBank::removeChannel(belle_sip_channel_t *channel) {
	auto &l = mChannelsById[normalizeIdentifier(belle_sip_channel_get_bank_identifier(channel))];
	auto it = std::find(l.begin(), l.end(), channel);
	if (it != l.end()) {
		l.erase(it);
	} else {
		belle_sip_error("hannelBank::removeChannel(): no channel [%p]", channel);
	}
}

size_t ChannelBank::removeChannelIf(int (*func)(belle_sip_channel_t *, void *), void *user_data) {
	size_t count = 0;
	for (auto &p : mChannelsById) {
		for (auto it = p.second.begin(); it != p.second.end();) {
			if (func(it->get(), user_data)) {
				it = p.second.erase(it);
				++count;
			} else ++it;
		}
	}
	return count;
}

std::string ChannelBank::normalizeIdentifier(const char *identifier) const {
	return identifier ? std::string(identifier) : "default";
}

belle_sip_channel_t *ChannelBank::findChannel(const std::list<SmartCPointer<belle_sip_channel_t>> &l,
                                              const belle_sip_hop_t *hop,
                                              const struct addrinfo *addr) const {
	for (auto &chanPointer : l) {
		belle_sip_channel_t *chan = chanPointer.get();
		if (chan->state == BELLE_SIP_CHANNEL_DISCONNECTED || chan->state == BELLE_SIP_CHANNEL_ERROR) continue;
		if (!chan->about_to_be_closed && belle_sip_channel_matches(chan, hop, addr)) {
			return chan;
		}
	}
	return nullptr;
}

belle_sip_channel_t *ChannelBank::findChannel(const belle_sip_hop_t *hop, const struct addrinfo *addr) const {
	if (hop) {
		std::string channelIdentifier = normalizeIdentifier(belle_sip_hop_get_channel_bank_identifier(hop));
		auto map_it = mChannelsById.find(channelIdentifier);

		if (map_it == mChannelsById.end()) return nullptr;
		return findChannel(map_it->second, hop, addr);
	}
	/* global search with hop, actually used only for incoming requests on UDP listening socket.*/
	for (auto &p : mChannelsById) {
		auto channel = findChannel(p.second, hop, addr);
		if (channel) return channel;
	}
	return nullptr;
}

belle_sip_channel_t *ChannelBank::findChannel(const belle_sip_uri_t *uri) const {
	for (auto &p : mChannelsById) {
		for (auto &elem : p.second) {
			belle_sip_uri_t *chan_uri = belle_sip_channel_create_routable_uri(elem.get());
			if (belle_sip_uri_get_port(uri) == belle_sip_uri_get_port(chan_uri) &&
			    0 == strcmp(belle_sip_uri_get_host(uri), belle_sip_uri_get_host(chan_uri))) {
				belle_sip_object_unref(chan_uri);
				return elem.get();
			}
			belle_sip_object_unref(chan_uri);
		}
	}
	return nullptr;
}

belle_sip_channel_t *ChannelBank::findChannel(int ai_family, const belle_sip_hop_t *hop) const {
	belle_sip_channel_t *chan = NULL;
	struct addrinfo *res = bctbx_ip_address_to_addrinfo(
	    ai_family, SOCK_STREAM /*needed on some platforms that return an error otherwise (QNX)*/, hop->host, hop->port);
	chan = findChannel(hop, res);
	if (res) bctbx_freeaddrinfo(res);
	return chan;
}

void ChannelBank::forEach(void (*func)(belle_sip_channel_t *, void *), void *user_data) {
	for (auto &p : mChannelsById) {
		for (auto &elem : p.second) {
			func(elem.get(), user_data);
		}
	}
}

size_t ChannelBank::getCount() const {
	size_t count = 0;
	for (auto &p : mChannelsById)
		count += p.second.size();
	return count;
}

void ChannelBank::clearAllChannels() {
	for (auto &p : mChannelsById) {
		for (auto &elem : p.second) {
			belle_sip_channel_force_close(elem.get());
		}
	}
	mChannelsById.clear();
}

ChannelBank::~ChannelBank() {
	clearAllChannels();
}

} // namespace bellesip

using namespace bellesip;

belle_sip_channel_bank_t *belle_sip_channel_bank_new(void) {
	return (new ChannelBank())->toC();
}

void belle_sip_channel_bank_add_channel(belle_sip_channel_bank_t *obj, belle_sip_channel_t *chan) {
	ChannelBank::toCpp(obj)->addChannel(chan);
}

void belle_sip_channel_bank_remove_channel(belle_sip_channel_bank_t *obj, belle_sip_channel_t *chan) {
	ChannelBank::toCpp(obj)->removeChannel(chan);
}

size_t belle_sip_channel_bank_remove_if(belle_sip_channel_bank_t *obj,
                                        int (*func)(belle_sip_channel_t *, void *),
                                        void *user_data) {
	return ChannelBank::toCpp(obj)->removeChannelIf(func, user_data);
}

size_t belle_sip_channel_bank_get_count(belle_sip_channel_bank_t *obj) {
	return ChannelBank::toCpp(obj)->getCount();
}

void belle_sip_channel_bank_clear_all(belle_sip_channel_bank_t *obj) {
	ChannelBank::toCpp(obj)->clearAllChannels();
}

belle_sip_channel_t *
belle_sip_channel_bank_find(belle_sip_channel_bank_t *obj, int ai_family, const belle_sip_hop_t *hop) {
	return ChannelBank::toCpp(obj)->findChannel(ai_family, hop);
}

belle_sip_channel_t *belle_sip_channel_bank_find_by_addrinfo(belle_sip_channel_bank_t *obj,
                                                             const struct addrinfo *addr) {
	return ChannelBank::toCpp(obj)->findChannel(nullptr, addr);
}

belle_sip_channel_t *belle_sip_channel_bank_find_by_local_uri(belle_sip_channel_bank_t *obj,
                                                              const belle_sip_uri_t *uri) {
	return ChannelBank::toCpp(obj)->findChannel(uri);
}

void belle_sip_channel_bank_for_each(belle_sip_channel_bank_t *obj,
                                     void (*func)(belle_sip_channel_t *, void *),
                                     void *user_data) {
	ChannelBank::toCpp(obj)->forEach(func, user_data);
}
