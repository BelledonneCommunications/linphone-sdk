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

#pragma once

#include <string>

#include <ortp/port.h>

#include "core/core-accessor.h"
#include "core/core.h"

#include "linphone/utils/general.h"

// =============================================================================

LINPHONE_BEGIN_NAMESPACE

class SalMediaDescription;

class StunClient : public CoreAccessor {
	struct Candidate {
		std::string address;
		int port = 0;
	};

public:
	explicit StunClient(const std::shared_ptr<Core> &core) : CoreAccessor(core) {
	}

	int run(int audioPort, int videoPort, int textPort);
	void updateMediaDescription(const std::shared_ptr<SalMediaDescription> &md) const;

	[[nodiscard]] const Candidate &getAudioCandidate() const {
		return audioCandidate;
	}

	[[nodiscard]] const Candidate &getVideoCandidate() const {
		return videoCandidate;
	}

	[[nodiscard]] const Candidate &getTextCandidate() const {
		return textCandidate;
	}

	static ortp_socket_t createStunSocket(int localPort);
	static int recvStunResponse(ortp_socket_t sock, Candidate &candidate, int &id);
	static int
	sendStunRequest(ortp_socket_t sock, const struct sockaddr *server, socklen_t addrlen, int id, bool changeAddr);

private:
	Candidate audioCandidate;
	Candidate videoCandidate;
	Candidate textCandidate;
	bool stunDiscoveryDone = false;
};

LINPHONE_END_NAMESPACE
