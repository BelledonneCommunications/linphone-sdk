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

#ifndef custom_dns_entries_h
#define custom_dns_entries_h

#include "belle_sip_internal.h"

#ifdef __cplusplus

#include "belle-sip/object++.hh"
#include <list>
#include <string>

namespace bellesip {

struct DnsRecord {
	std::string name;
	int type = 0; // DNS_T_A, DNS_T_AAAA, DNS_T_SRV
	int ttl = 0;
	std::string ip;     // for A / AAAA
	std::string target; // for SRV
	int port = 0;       // for SRV
	int priority = 0;   // for SRV
	int weight = 0;     // for SRV
};

class CustomDnsEntries : public HybridObject<belle_sip_custom_dns_entries_t, CustomDnsEntries> {
public:
	CustomDnsEntries() = default;
	CustomDnsEntries(const CustomDnsEntries &) = delete;

	void addA(const std::string &name, const std::string &ip, int ttl);
	void addAAAA(const std::string &name, const std::string &ip, int ttl);
	void addSrv(const std::string &service,
	            const std::string &transport,
	            const std::string &domain,
	            const std::string &target,
	            int port,
	            int priority,
	            int weight,
	            int ttl);
	void clear();

	bool resolve(belle_sip_simple_resolver_context_t *ctx) const;

protected:
	~CustomDnsEntries() override = default;

private:
	std::list<DnsRecord> mRecords;
};

} // namespace bellesip

extern "C" {
#endif /* __cplusplus */

BELLE_SIP_BEGIN_DECLS

belle_sip_custom_dns_entries_t *belle_sip_custom_dns_entries_new(void);

void belle_sip_custom_dns_entries_add_a(belle_sip_custom_dns_entries_t *entries,
                                        const char *name,
                                        const char *ip,
                                        int ttl);

void belle_sip_custom_dns_entries_add_aaaa(belle_sip_custom_dns_entries_t *entries,
                                           const char *name,
                                           const char *ip,
                                           int ttl);

void belle_sip_custom_dns_entries_add_srv(belle_sip_custom_dns_entries_t *entries,
                                          const char *service,
                                          const char *transport,
                                          const char *domain,
                                          const char *target,
                                          int port,
                                          int priority,
                                          int weight,
                                          int ttl);

void belle_sip_custom_dns_entries_clear(belle_sip_custom_dns_entries_t *entries);

int belle_sip_custom_dns_entries_resolve(belle_sip_custom_dns_entries_t *entries,
                                         belle_sip_simple_resolver_context_t *ctx);

BELLE_SIP_END_DECLS

#ifdef __cplusplus
}
#endif

#endif /* custom_dns_entries_h */
