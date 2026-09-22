/*
 * Copyright (c) 2010-2026 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
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

#include <memory>
#include <optional>
#include <string>

#include "mediastreamer2/ice-constants.h"
#include "mediastreamer2/ice-transport-address.h"
#include "mediastreamer2/mscommon.h"

namespace mediastreamer::nat {

/**
 * Represents an ICE candidate.
 */
class MS2_PUBLIC IceCandidate {
public:
	friend class IceCandidatePair;
	friend class IceCheckList;

	/**
	 * ICE candidate type.
	 *
	 * See the terminology in paragraph 3 of the RFC 5245 for more details.
	 */
	enum class Type {
		Host = 0,
		ServerReflexive,
		PeerReflexive,
		Relayed,
	};

	~IceCandidate() = default;

	bool operator==(const IceCandidate &other) const;

	/**
	 * Get the base candidate of an ICE candidate.
	 * @return A pointer to the base candidate of the ICE candidate.
	 */
	[[nodiscard]] std::shared_ptr<IceCandidate> getBase() const;

	/**
	 * Get the component ID of an ICE candidate.
	 * @return The component ID of the ICE candidate.
	 */
	[[nodiscard]] ComponentId getComponentId() const {
		return mComponentId;
	}

	/**
	 * Get the foundation of an ICE candidate.
	 * @return The foundation of the ICE candidate.
	 */
	[[nodiscard]] const std::string &getFoundation() const {
		return mFoundation;
	}

	/**
	 * Get the priority of an ICE candidate.
	 * @return The priority of the ICE candidate.
	 */
	[[nodiscard]] uint32_t getPriority() const {
		return mPriority;
	}

	/**
	 * Get the transport address of an ICE candidate.
	 * @return The transport address of the ICE candidate.
	 */
	[[nodiscard]] const IceTransportAddress &getTransportAddress() const {
		return mTransportAddress;
	}

	/**
	 * Get the type of an ICE candidate.
	 * @return The type of the ICE candidate.
	 */
	[[nodiscard]] Type getType() const {
		return mType;
	}

	/**
	 * Get the candidate type as a string.
	 * @return The candidate type as a string
	 */
	[[nodiscard]] const std::string &getTypeStr() const {
		return getTypeStr(mType);
	}

	[[nodiscard]] bool isDefault() const {
		return mIsDefault;
	}
	[[nodiscard]] bool isHost() const {
		return mType == Type::Host;
	}
	[[nodiscard]] bool isRelay() const;
	void setBase(const std::shared_ptr<IceCandidate> &base);

	[[nodiscard]] static std::shared_ptr<IceCandidate>
	create(Type type, const IceTransportAddress &transportAddress, ComponentId componentId);
	[[nodiscard]] static std::optional<Type> getTypeFromStr(const std::string &typeStr);
	[[nodiscard]] static const std::string &getTypeStr(Type type);

private:
	IceCandidate(Type type, const IceTransportAddress &transportAddress, ComponentId componentId);

	void computePriority();
	void dump(const std::string &prefix) const;
	void setDefault(const bool isDefault) {
		mIsDefault = isDefault;
	}
	void setFoundation(const std::string &foundation) {
		mFoundation = foundation;
	}
	void setPriority(const uint32_t priority) {
		mPriority = priority;
	}

	static uint8_t getTypePreferenceValue(Type type);

	std::string mFoundation; /**< Foundation of the candidate (see paragraph 3 of the RFC 5245 for more details) */
	Type mType;              /**< Type of the candidate */
	IceTransportAddress mTransportAddress; /**< Transport address of the candidate */
	uint32_t mPriority = 0;                /**< Priority of the candidate */
	ComponentId
	    mComponentId; /**< component ID between 1 and 256: usually 1 for RTP component and 2 for RTCP component */
	std::weak_ptr<IceCandidate> mBase; /**< Pointer to the candidate that is the base of the current one */
	bool mIsDefault = false; /**< Boolean value telling whether this candidate is a default candidate or not */
};

} // namespace mediastreamer::nat
