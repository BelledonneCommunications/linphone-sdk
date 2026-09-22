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

#include <array>
#include <utility>
#include <vector>

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/stun-message.h"

namespace mediastreamer::nat {

class MS2_PUBLIC StunRawMessage {
public:
	friend class StunMessage;

	~StunRawMessage() = default;

	[[nodiscard]] bool
	checkShortTermIntegrity(const std::string &password,
	                        const std::array<uint8_t, StunMessage::kMessageIntegrityLength> &expectedIntegrity);
	[[nodiscard]] const std::vector<uint8_t> &getData() const {
		return mData;
	}

	static constexpr size_t kMaxRawMessageLength = 2048;

private:
	enum class Attribute {
		StunMappedAddress = 0x0001,
		StunResponseAddress = 0x0002, // Deprecated, now reserved
		StunChangeRequest = 0x0003,   // Deprecated, now reserved
		StunSourceAddress = 0x0004,   // Deprecated, now reserved
		StunChangedAddress = 0x0005,  // Deprecated, now reserved
		StunUsername = 0x0006,
		StunPassword = 0x0007, // Deprecated, now reserved
		StunMessageIntegrity = 0x0008,
		StunErrorCode = 0x0009,
		StunUnknownAttributes = 0x000A,
		StunReflectedFrom = 0x000B,
		StunRealm = 0x0014,
		StunNonce = 0x0015,
		StunXorMappedAddress = 0x0020,
		StunSoftware = 0x8022,
		StunAlternateServer = 0x8023,
		StunFingerprint = 0x8028,

		TurnChannelNumber = 0x000C,
		TurnLifetime = 0x000D,
		TurnBandwidth = 0x0010, // Deprecated, now reserved
		TurnXorPeerAddress = 0x0012,
		TurnData = 0x0013,
		TurnXorRelayedAddress = 0x0016,
		TurnRequestedAddressFamily = 0x0017,
		TurnEvenPort = 0x0018,
		TurnRequestedTransport = 0x0019,
		TurnDontFragment = 0x001A,
		TurnTimerVal = 0x0021, // Deprecated, now reserved
		TurnReservationToken = 0x0022,

		IcePriority = 0x0024,
		IceUseCandidate = 0x0025,
		IceControlled = 0x8029,
		IceControlling = 0x802A,
	};

	static constexpr uint32_t kFlagChangeIp = 0x04;
	static constexpr uint32_t kFlagChangePort = 0x02;
	static constexpr size_t kMaxNonceLength = 127;
	static constexpr size_t kMaxRealmLength = 127;
	static constexpr size_t kMaxSoftwareLength =
	    763; // Length in bytes, it is supposed to be less than 128 UTF-8 characters (TODO)
	static constexpr size_t kMaxUsernameLength = 513;
	static constexpr size_t kMessageHeaderLength = 20;

	StunRawMessage();
	StunRawMessage(const char *data, size_t len);

	void addAttribute(Attribute attribute, bool value);
	void addAttribute(Attribute attribute, uint8_t value);
	void addAttribute(Attribute attribute, uint16_t value);
	void addAttribute(Attribute attribute, uint32_t value);
	void addAttribute(Attribute attribute, uint64_t value);
	void addAttribute(Attribute attribute, const std::string &value);
	void addAttribute(Attribute attribute, const std::vector<uint8_t> &value);
	void addAttribute(Attribute attribute, const std::array<uint8_t, StunMessage::kMessageIntegrityLength> &value);
	void addAttribute(Attribute attribute, const StunAddress &address);
	void addAttribute(Attribute attribute, const StunError &error);
	void addFingerprint();
	void addHeader(StunMessage::Type type, StunMessage::Method method, const StunTransactionId &transactionId);
	void addLongTermIntegrity(const std::string &realm, const std::string &username, const std::string &password);
	void addLongTermIntegrityFromHa1(const std::string &ha1);
	void addShortTermIntegrity(const std::string &password, bool hasDummyMessageIntegrity);
	[[nodiscard]] uint32_t calculateFingerprint() const;
	[[nodiscard]] std::array<uint8_t, StunMessage::kMessageIntegrityLength>
	calculateLongTermIntegrityFromHa1(const std::string &ha1) const;
	[[nodiscard]] std::array<uint8_t, StunMessage::kMessageIntegrityLength> calculateLongTermIntegrity(
	    const std::string &realm, const std::string &username, const std::string &password) const;
	[[nodiscard]] std::array<uint8_t, StunMessage::kMessageIntegrityLength>
	calculateShortTermIntegrity(const std::string &password, size_t length = 0) const;
	[[nodiscard]] const uint8_t *decode(size_t size);
	[[nodiscard]] std::pair<Attribute, size_t> decodeAttributeHeader();
	[[nodiscard]] uint16_t decode16();
	[[nodiscard]] uint32_t decode32();
	[[nodiscard]] uint64_t decode64();
	[[nodiscard]] uint8_t decode8();
	[[nodiscard]] StunAddress decodeAddress(size_t length);
	[[nodiscard]] uint32_t decodeChangeRequest(size_t length);
	[[nodiscard]] StunError decodeErrorCode(size_t length);
	[[nodiscard]] uint32_t decodeFingerprint(size_t length);
	[[nodiscard]] uint64_t decodeIceControlled(size_t length);
	[[nodiscard]] uint64_t decodeIceControlling(size_t length);
	[[nodiscard]] uint32_t decodeLifetime(size_t length);
	[[nodiscard]] std::array<uint8_t, StunMessage::kMessageIntegrityLength> decodeMessageIntegrity(size_t length);
	[[nodiscard]] uint32_t decodePriority(size_t length);
	[[nodiscard]] std::string decodeString(size_t length, size_t maxLength);
	void encode(const uint8_t *data, size_t size);
	void encode16(uint16_t value);
	void encode32(uint32_t value);
	void encode64(uint64_t value);
	void encode8(uint8_t value);
	void encodeAttribute(Attribute attribute);
	void encodePadding(size_t size);
	void ensureCapacity(size_t size);
	void ensureEnoughDataForDecoding(size_t size) const;
	[[nodiscard]] const uint8_t *getDecodePtr() const;
	[[nodiscard]] size_t getMessageLength() const;
	[[nodiscard]] std::shared_ptr<StunMessage> parse();
	[[nodiscard]] std::shared_ptr<StunMessage> parseHeader();
	void setMessageLength(size_t length);
	void updateMessageLength(size_t additionalLength = 0);

	std::vector<uint8_t> mData;
	size_t mDecodeIndex = 0;
};

}; // namespace mediastreamer::nat