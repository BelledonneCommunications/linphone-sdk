/*
 * Copyright (c) 2026-? Belledonne Communications SARL.
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

#include "custom_dns_entries.hh"

#include <bctoolbox/port.h>
#include <cctype>

#ifndef DNS_T_A
#define DNS_T_A 1
#endif
#ifndef DNS_T_AAAA
#define DNS_T_AAAA 28
#endif
#ifndef DNS_T_SRV
#define DNS_T_SRV 33
#endif

namespace bellesip {

static std::string normalizeName(const std::string &name) {
	std::string res;
	res.reserve(name.size());
	for (char c : name) {
		res.push_back((char)tolower((unsigned char)c));
	}
	while (!res.empty() && res.back() == '.') {
		res.pop_back();
	}
	return res;
}

void CustomDnsEntries::addA(const std::string &name, const std::string &ip, int ttl) {
	DnsRecord rec;
	rec.name = name;
	rec.type = DNS_T_A;
	rec.ip = ip;
	rec.ttl = ttl;
	mRecords.push_back(std::move(rec));
}

void CustomDnsEntries::addAAAA(const std::string &name, const std::string &ip, int ttl) {
	DnsRecord rec;
	rec.name = name;
	rec.type = DNS_T_AAAA;
	rec.ip = ip;
	rec.ttl = ttl;
	mRecords.push_back(std::move(rec));
}

void CustomDnsEntries::addSrv(const std::string &service,
                              const std::string &transport,
                              const std::string &domain,
                              const std::string &target,
                              int port,
                              int priority,
                              int weight,
                              int ttl) {
	std::string s = service.empty() ? "sip" : service;
	std::string prefix;
	if (strcasecmp(transport.c_str(), "tls") == 0) {
		prefix = "_" + s + "s._tcp.";
	} else if (strcasecmp(transport.c_str(), "tcp") == 0) {
		prefix = "_" + s + "._tcp.";
	} else {
		prefix = "_" + s + "._" + transport + ".";
	}

	DnsRecord rec;
	rec.name = prefix + domain;
	rec.type = DNS_T_SRV;
	rec.target = target;
	rec.port = port;
	rec.priority = priority;
	rec.weight = weight;
	rec.ttl = ttl;
	mRecords.push_back(std::move(rec));
}

void CustomDnsEntries::clear() {
	mRecords.clear();
}

bool CustomDnsEntries::resolve(belle_sip_simple_resolver_context_t *ctx) const {
	const char *rawName = belle_sip_simple_resolver_context_get_name(ctx);
	if (!rawName) return false;

	std::string queryName = normalizeName(rawName);
	int queryType = belle_sip_simple_resolver_context_get_type(ctx);
	int port = belle_sip_simple_resolver_context_get_port(ctx);
	int family = belle_sip_simple_resolver_context_get_family(ctx);
	int flags = belle_sip_simple_resolver_context_get_flags(ctx);
	if (flags & AI_V4MAPPED) family = AF_INET6;

	bool hasHost = false;
	bool foundType = false;
	uint32_t minTtl = UINT32_MAX;

	for (const auto &rec : mRecords) {
		if (normalizeName(rec.name) == queryName) {
			hasHost = true;
			if (rec.type == queryType) {
				foundType = true;
				if (rec.ttl > 0 && (uint32_t)rec.ttl < minTtl) {
					minTtl = (uint32_t)rec.ttl;
				}
				if (queryType == DNS_T_A || queryType == DNS_T_AAAA) {
					struct addrinfo *ai = bctbx_ip_address_to_addrinfo(family, SOCK_STREAM, rec.ip.c_str(), port);
					if (ai) {
						belle_sip_simple_resolver_context_add_addrinfo(ctx, ai);
						belle_sip_message("Custom DNS resolved %s (%s) to %s", rawName,
						                  (queryType == DNS_T_A ? "A" : "AAAA"), rec.ip.c_str());
					}
				} else if (queryType == DNS_T_SRV) {
					belle_sip_dns_srv_t *srv =
					    belle_sip_dns_srv_create_custom((unsigned short)rec.priority, (unsigned short)rec.weight,
					                                    (unsigned short)rec.port, rec.target.c_str());
					belle_sip_simple_resolver_context_add_srv(ctx, srv);
					belle_sip_message("Custom DNS resolved SRV %s to [target:%s port:%d prio:%d weight:%d]", rawName,
					                  rec.target.c_str(), rec.port, rec.priority, rec.weight);
				}
			}
		}
	}

	if (hasHost) {
		if (foundType && minTtl != UINT32_MAX) {
			belle_sip_simple_resolver_context_set_min_ttl(ctx, minTtl);
		}
		if (!foundType) {
			belle_sip_message("Custom DNS host %s found, but no record of type %d", rawName, queryType);
		}
		return true;
	}
	return false;
}

} // namespace bellesip

using namespace bellesip;

belle_sip_custom_dns_entries_t *belle_sip_custom_dns_entries_new(void) {
	return (new CustomDnsEntries())->toC();
}

void belle_sip_custom_dns_entries_add_a(belle_sip_custom_dns_entries_t *entries,
                                        const char *name,
                                        const char *ip,
                                        int ttl) {
	CustomDnsEntries::toCpp(entries)->addA(name ? name : "", ip ? ip : "", ttl);
}

void belle_sip_custom_dns_entries_add_aaaa(belle_sip_custom_dns_entries_t *entries,
                                           const char *name,
                                           const char *ip,
                                           int ttl) {
	CustomDnsEntries::toCpp(entries)->addAAAA(name ? name : "", ip ? ip : "", ttl);
}

void belle_sip_custom_dns_entries_add_srv(belle_sip_custom_dns_entries_t *entries,
                                          const char *service,
                                          const char *transport,
                                          const char *domain,
                                          const char *target,
                                          int port,
                                          int priority,
                                          int weight,
                                          int ttl) {
	CustomDnsEntries::toCpp(entries)->addSrv(service ? service : "sip", transport ? transport : "udp",
	                                         domain ? domain : "", target ? target : "", port, priority, weight, ttl);
}

void belle_sip_custom_dns_entries_clear(belle_sip_custom_dns_entries_t *entries) {
	CustomDnsEntries::toCpp(entries)->clear();
}

int belle_sip_custom_dns_entries_resolve(belle_sip_custom_dns_entries_t *entries,
                                         belle_sip_simple_resolver_context_t *ctx) {
	return CustomDnsEntries::toCpp(entries)->resolve(ctx) ? 1 : 0;
}
