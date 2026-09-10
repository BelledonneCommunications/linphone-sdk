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

#include <array>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <bctoolbox/crypto.h>
#include <bctoolbox/defs.h>

#include "mediastreamer2/stun-address.h"
#include "mediastreamer2/stun-message.h"
#include "mediastreamer2/stun-raw-message.h"

#if defined(htonq)
#elif defined(WORDS_BIGENDIAN)
#define htonq(n) n
#define ntohq(n) n
#else  /* little endian */
static BCTBX_INLINE uint64_t htonq(uint64_t v) {
	return htonl(static_cast<uint32_t>(v >> 32)) | static_cast<uint64_t>(htonl(static_cast<uint32_t>(v))) << 32;
}
static BCTBX_INLINE uint64_t ntohq(uint64_t v) {
	return ntohl(static_cast<uint32_t>(v >> 32)) | static_cast<uint64_t>(ntohl(static_cast<uint32_t>(v))) << 32;
}
#endif /* little endian */

namespace ms2::nat {

static constexpr size_t DEFAULT_STUN_RAW_MESSAGE_SIZE = 128;
static constexpr size_t STUN_MESSAGE_LENGTH_INDEX = 2;

bool StunRawMessage::checkShortTermIntegrity(
    const std::string &password, const std::array<uint8_t, StunMessage::MESSAGE_INTEGRITY_LENGTH> &expectedIntegrity) {
	const auto initialMessageLength = getMessageLength();
	// First remove length of fingerprint...
	setMessageLength(initialMessageLength - 8);

	const auto hmac = calculateShortTermIntegrity(password, mData.size() - 24 - 8);

	// ... and then restore the length with fingerprint.
	setMessageLength(initialMessageLength);

	return hmac == expectedIntegrity;
}

StunRawMessage::StunRawMessage() {
	mData.reserve(DEFAULT_STUN_RAW_MESSAGE_SIZE);
}

StunRawMessage::StunRawMessage(const char *data, const size_t len) {
	mData.assign(data, data + len);
}

void StunRawMessage::addAttribute(const Attribute attribute, BCTBX_UNUSED(const bool value)) {
	encodeAttribute(attribute);
	encode16(0);
}

void StunRawMessage::addAttribute(const Attribute attribute, const uint8_t value) {
	encodeAttribute(attribute);
	encode16(4);
	encode8(value);
	encode8(0);
	encode16(0);
}

void StunRawMessage::addAttribute(const Attribute attribute, const uint16_t value) {
	encodeAttribute(attribute);
	encode16(4);
	encode16(value);
	encode16(0);
}

void StunRawMessage::addAttribute(const Attribute attribute, const uint32_t value) {
	encodeAttribute(attribute);
	encode16(4);
	encode32(value);
}

void StunRawMessage::addAttribute(const Attribute attribute, const uint64_t value) {
	encodeAttribute(attribute);
	encode16(8);
	encode64(value);
}

void StunRawMessage::addAttribute(const Attribute attribute, const std::string &value) {
	encodeAttribute(attribute);
	encode16(static_cast<uint16_t>(value.size()));
	encode(reinterpret_cast<const uint8_t *>(value.c_str()), value.size());
	encodePadding(value.size());
}

void StunRawMessage::addAttribute(const Attribute attribute, const std::vector<uint8_t> &value) {
	encodeAttribute(attribute);
	encode16(static_cast<uint16_t>(value.size()));
	encode(value.data(), value.size());
	encodePadding(value.size());
}

void StunRawMessage::addAttribute(const Attribute attribute,
                                  const std::array<uint8_t, StunMessage::MESSAGE_INTEGRITY_LENGTH> &value) {
	encodeAttribute(attribute);
	encode16(static_cast<uint16_t>(value.size()));
	encode(value.data(), value.size());
	encodePadding(value.size());
}

void StunRawMessage::addAttribute(const Attribute attribute, const StunAddress &address) {
	encodeAttribute(attribute);
	switch (address.getFamily()) {
		default:
		case StunAddress::Family::IpV4:
			encode16(8);
			break;
		case StunAddress::Family::IpV6:
			encode16(20);
			break;
	}
	encode8(0);
	encode8(static_cast<uint8_t>(address.getFamily()));
	encode16(address.mPort);
	switch (address.getFamily()) {
		default:
		case StunAddress::Family::IpV4:
			encode32(address.getIpV4Address().value());
			break;
		case StunAddress::Family::IpV6:
			encode(reinterpret_cast<const uint8_t *>(address.getIpV6Address().value().octet), sizeof(StunAddressV6));
			break;
	}
}

void StunRawMessage::addAttribute(Attribute attribute, const StunError &error) {
	const auto errorCode = static_cast<uint16_t>(error.getErrorCode());
	const auto reason = error.getReason();
	encodeAttribute(attribute);
	encode16(static_cast<uint16_t>(4 + reason.size()));
	encode16(0);
	encode8(errorCode / 100);
	encode8(errorCode - ((errorCode / 100) * 100));
	encode(reinterpret_cast<const uint8_t *>(reason.c_str()), reason.size());
	encodePadding(reason.size());
}

void StunRawMessage::addFingerprint() {
	updateMessageLength(8);
	const uint32_t fingerprint = calculateFingerprint() ^ 0x5354554E;
	addAttribute(Attribute::StunFingerprint, fingerprint);
}

void StunRawMessage::addHeader(const StunMessage::Type type,
                               const StunMessage::Method method,
                               const StunTransactionId &transactionId) {
	encode16(static_cast<uint16_t>(type) | static_cast<uint16_t>(method));
	encode16(0); // Initialize length to 0, it will be updated later
	encode32(STUN_MAGIC_COOKIE);
	encode(reinterpret_cast<const uint8_t *>(&transactionId.asUInt96()), sizeof(UInt96));
}

void StunRawMessage::addLongTermIntegrity(const std::string &realm,
                                          const std::string &username,
                                          const std::string &password) {
	updateMessageLength(24);
	addAttribute(Attribute::StunMessageIntegrity, calculateLongTermIntegrity(realm, username, password));
}

void StunRawMessage::addLongTermIntegrityFromHa1(const std::string &ha1) {
	updateMessageLength(24);
	addAttribute(Attribute::StunMessageIntegrity, calculateLongTermIntegrityFromHa1(ha1));
}

void StunRawMessage::addShortTermIntegrity(const std::string &password, const bool hasDummyMessageIntegrity) {
	updateMessageLength(24);
	std::array<uint8_t, StunMessage::MESSAGE_INTEGRITY_LENGTH> hmac{};
	if (hasDummyMessageIntegrity) {
		hmac = StunMessage::DUMMY_MESSAGE_INTEGRITY;
		BCTBX_SLOGW << "hmac not implemented by remote, using dummy integrity hash for stun message";
	} else {
		hmac = calculateShortTermIntegrity(password);
	}
	addAttribute(Attribute::StunMessageIntegrity, hmac);
}

uint32_t StunRawMessage::calculateFingerprint() const {
	static constexpr std::array<uint32_t, 256> crc32Tab = {
	    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832,
	    0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856, 0x646ba8c0, 0xfd62f97a,
	    0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3,
	    0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab,
	    0xb6662d3d, 0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01, 0x6b6b51f4,
	    0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65, 0x4db26158, 0x3ab551ce, 0xa3bc0074,
	    0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525,
	    0x206f85b3, 0xb966d409, 0xce61e49f, 0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615,
	    0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7, 0xfed41b76,
	    0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c, 0x36034af6,
	    0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7,
	    0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7,
	    0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45, 0xa00ae278,
	    0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9, 0xbdbdf21c, 0xcabac28a, 0x53b39330,
	    0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,
	};

	uint32_t crc = ~0;
	for (auto p : mData) {
		crc = crc32Tab[(crc ^ p) & 0xff] ^ (crc >> 8);
	}
	return crc ^ ~0;
}

std::array<uint8_t, StunMessage::MESSAGE_INTEGRITY_LENGTH> StunRawMessage::calculateLongTermIntegrity(
    const std::string &realm, const std::string &username, const std::string &password) const {
	std::array<uint8_t, 16> ha1Bytes{};
	const std::string ha1 = username + ":" + realm + ":" + password;
	bctbx_md5(reinterpret_cast<const uint8_t *>(ha1.data()), ha1.size(), ha1Bytes.data());
	// SHA1 output length is 20 bytes, get them all
	std::array<uint8_t, StunMessage::MESSAGE_INTEGRITY_LENGTH> hmac{};
	bctbx_hmacSha1(ha1Bytes.data(), ha1Bytes.size(), reinterpret_cast<const uint8_t *>(mData.data()), mData.size(),
	               static_cast<uint8_t>(hmac.size()), reinterpret_cast<uint8_t *>(hmac.data()));
	return hmac;
}

std::array<uint8_t, StunMessage::MESSAGE_INTEGRITY_LENGTH>
StunRawMessage::calculateLongTermIntegrityFromHa1(const std::string &ha1) const {
	std::array<uint8_t, 16> ha1Bytes{};
	for (size_t i = 0, j = 0; (i < ha1.size()) && (j < ha1Bytes.size()); i += 2, j++) {
		char buf[3] = {ha1[i], ha1[i + 1], '\0'};
		ha1Bytes[j] = static_cast<uint8_t>(strtol(buf, nullptr, 16));
	}
	// SHA1 output length is 20 bytes, get them all
	std::array<uint8_t, StunMessage::MESSAGE_INTEGRITY_LENGTH> hmac{};
	bctbx_hmacSha1(ha1Bytes.data(), ha1Bytes.size(), reinterpret_cast<const uint8_t *>(mData.data()), mData.size(),
	               static_cast<uint8_t>(hmac.size()), reinterpret_cast<uint8_t *>(hmac.data()));
	return hmac;
}

std::array<uint8_t, StunMessage::MESSAGE_INTEGRITY_LENGTH>
StunRawMessage::calculateShortTermIntegrity(const std::string &password, size_t length) const {
	if (length == 0) {
		length = mData.size();
	}
	// SHA1 output length is 20 bytes, get them all
	std::array<uint8_t, StunMessage::MESSAGE_INTEGRITY_LENGTH> hmac{};
	bctbx_hmacSha1(reinterpret_cast<const uint8_t *>(password.data()), password.size(),
	               reinterpret_cast<const uint8_t *>(mData.data()), length, static_cast<uint8_t>(hmac.size()),
	               reinterpret_cast<uint8_t *>(hmac.data()));
	return hmac;
}

const uint8_t *StunRawMessage::decode(size_t size) {
	ensureEnoughDataForDecoding(size);
	const uint8_t *ptr = getDecodePtr();
	mDecodeIndex += size;
	return ptr;
}

uint16_t StunRawMessage::decode16() {
	ensureEnoughDataForDecoding(sizeof(uint16_t));
	const uint16_t value = ntohs(*(reinterpret_cast<const uint16_t *>(getDecodePtr())));
	mDecodeIndex += sizeof(uint16_t);
	return value;
}

uint32_t StunRawMessage::decode32() {
	ensureEnoughDataForDecoding(sizeof(uint32_t));
	const uint32_t value = ntohl(*(reinterpret_cast<const uint32_t *>(getDecodePtr())));
	mDecodeIndex += sizeof(uint32_t);
	return value;
}

uint64_t StunRawMessage::decode64() {
	ensureEnoughDataForDecoding(sizeof(uint64_t));
	const uint64_t value = ntohq(*(reinterpret_cast<const uint64_t *>(getDecodePtr())));
	mDecodeIndex += sizeof(uint64_t);
	return value;
}

uint8_t StunRawMessage::decode8() {
	ensureEnoughDataForDecoding(sizeof(uint8_t));
	const uint8_t value = *(getDecodePtr());
	mDecodeIndex += sizeof(uint8_t);
	return value;
}

StunAddress StunRawMessage::decodeAddress(const size_t length) {
	if ((length != 8) && (length != 20)) {
		throw std::runtime_error("STUN address attribute with wrong length");
	}
	std::ignore = decode8();
	StunAddress stunAddress{};
	const auto family = static_cast<StunAddress::Family>(decode8());
	stunAddress.mPort = decode16();
	if (family == StunAddress::Family::IpV6) {
		StunAddressV6 addr{};
		memcpy(&addr, decode(sizeof(StunAddressV6)), sizeof(StunAddressV6));
		stunAddress.setAddress(addr);
	} else {
		stunAddress.setAddress(decode32());
	}
	return stunAddress;
}

std::pair<StunRawMessage::Attribute, size_t> StunRawMessage::decodeAttributeHeader() {
	const auto attribute = static_cast<Attribute>(decode16());
	const auto length = static_cast<size_t>(decode16());
	if (length > (mData.size() - mDecodeIndex)) {
		std::ostringstream oss;
		oss << "STUN attribute larger than message (attribute type: 0x" << std::hex << std::setfill('0') << std::setw(4)
		    << static_cast<uint16_t>(attribute) << ")";
		throw std::runtime_error(oss.str());
	}
	return {attribute, length};
}

uint32_t StunRawMessage::decodeChangeRequest(const size_t length) {
	if (length != 4) {
		throw std::runtime_error("STUN change address attribute with wrong length");
	}
	return decode32();
}

StunError StunRawMessage::decodeErrorCode(const size_t length) {
	if ((length < 4) || (length > (StunError::MAX_REASON_LENGTH + 4))) {
		throw std::runtime_error("STUN error code attribute with wrong length");
	}

	const auto reasonLength = length - 4;
	std::ignore = decode16();
	const auto errorCode = static_cast<StunError::Code>((decode8() * 100) + decode8());
	const auto reason = std::string(reinterpret_cast<const char *>(getDecodePtr()), reasonLength);
	mDecodeIndex += reasonLength;
	return StunError(errorCode, reason);
}

uint32_t StunRawMessage::decodeFingerprint(const size_t length) {
	if (length != 4) {
		throw std::runtime_error("STUN fingerprint attribute with wrong length");
	}
	return decode32();
}

uint64_t StunRawMessage::decodeIceControlled(const size_t length) {
	if (length != 8) {
		throw std::runtime_error("STUN ice controlled attribute with wrong length");
	}
	return decode64();
}

uint64_t StunRawMessage::decodeIceControlling(const size_t length) {
	if (length != 8) {
		throw std::runtime_error("STUN ice controlling attribute with wrong length");
	}
	return decode64();
}

uint32_t StunRawMessage::decodeLifetime(const size_t length) {
	if (length != 4) {
		throw std::runtime_error("STUN lifetime attribute with wrong length");
	}
	return decode32();
}

std::array<uint8_t, StunMessage::MESSAGE_INTEGRITY_LENGTH> StunRawMessage::decodeMessageIntegrity(const size_t length) {
	if (length != StunMessage::MESSAGE_INTEGRITY_LENGTH) {
		throw std::runtime_error("STUN message integrity attribute with wrong length");
	}
	auto result = std::array<uint8_t, StunMessage::MESSAGE_INTEGRITY_LENGTH>{};
	memcpy(reinterpret_cast<uint8_t *>(result.data()), getDecodePtr(), length);
	mDecodeIndex += length;
	return result;
}

uint32_t StunRawMessage::decodePriority(const size_t length) {
	if (length != 4) {
		throw std::runtime_error("STUN priority attribute with wrong length");
	}
	return decode32();
}

std::string StunRawMessage::decodeString(const size_t length, const size_t maxLength) {
	if (length > maxLength) {
		throw std::runtime_error("STUN string attribute too long");
	}
	auto result = std::string(reinterpret_cast<const char *>(getDecodePtr()), length);
	mDecodeIndex += length;
	return result;
}

void StunRawMessage::encode(const uint8_t *data, const size_t size) {
	ensureCapacity(size);
	mData.insert(mData.end(), data, data + size);
}

void StunRawMessage::encode16(const uint16_t value) {
	const uint16_t networkValue = htons(value);
	encode(reinterpret_cast<const uint8_t *>(&networkValue), sizeof(uint16_t));
}

void StunRawMessage::encode32(const uint32_t value) {
	const uint32_t networkValue = htonl(value);
	encode(reinterpret_cast<const uint8_t *>(&networkValue), sizeof(uint32_t));
}

void StunRawMessage::encode64(const uint64_t value) {
	const uint64_t networkValue = htonq(value);
	encode(reinterpret_cast<const uint8_t *>(&networkValue), sizeof(uint64_t));
}

void StunRawMessage::encode8(const uint8_t value) {
	ensureCapacity(sizeof(value));
	mData.push_back(value);
}

void StunRawMessage::encodeAttribute(const Attribute attribute) {
	encode16(static_cast<uint16_t>(attribute));
}

void StunRawMessage::encodePadding(const size_t size) {
	const size_t padding = 4 - (size % 4);
	if (padding < 4) {
		for (size_t i = 0; i < padding; i++) {
			encode8(0);
		}
	}
}

void StunRawMessage::ensureCapacity(const size_t size) {
	if (size == 0) {
		return;
	}
	if ((mData.capacity() - mData.size()) < size) {
		if (size < mData.capacity()) {
			mData.reserve(mData.capacity() * 2);
		} else {
			mData.reserve(mData.capacity() + size);
		}
	}
}

void StunRawMessage::ensureEnoughDataForDecoding(const size_t size) const {
	if ((mDecodeIndex + size) > mData.size()) {
		throw std::out_of_range("Not enough data for decoding");
	}
}

[[nodiscard]] const uint8_t *StunRawMessage::getDecodePtr() const {
	return mData.data() + mDecodeIndex;
}

size_t StunRawMessage::getMessageLength() const {
	uint16_t networkLength = 0;
	memcpy(&networkLength, &mData[STUN_MESSAGE_LENGTH_INDEX], sizeof(uint16_t));
	return ntohs(networkLength);
}

std::shared_ptr<StunMessage> StunRawMessage::parse() {
	try {
		auto stunMessage = parseHeader();
		// BCTBX_SLOGW << "GMA: TransactionId: " << stunMessage->getTransactionId().asString();

		while (mDecodeIndex < mData.size()) {
			const auto [attribute, length] = decodeAttributeHeader();
			switch (attribute) {
				case Attribute::StunMappedAddress:
					// BCTBX_SLOGW << "\tStunMappedAddress";
					stunMessage->setMappedAddress(decodeAddress(length));
					break;
				case Attribute::StunChangeRequest: {
					// BCTBX_SLOGW << "\tStunChangeRequest";
					uint32_t changeRequest = decodeChangeRequest(length);
					if ((changeRequest & FLAG_CHANGE_IP) == FLAG_CHANGE_IP) {
						stunMessage->enableChangeIp(true);
					}
					if ((changeRequest & FLAG_CHANGE_PORT) == FLAG_CHANGE_PORT) {
						stunMessage->enableChangePort(true);
					}
				} break;
				case Attribute::StunResponseAddress:
				case Attribute::StunSourceAddress:
				case Attribute::StunChangedAddress:
					// Ignore these deprecated attributes.
					std::ignore = decodeAddress(length);
					break;
				case Attribute::StunUsername:
					stunMessage->setUsername(decodeString(length, MAX_USERNAME_LENGTH));
					// BCTBX_SLOGW << "\tStunUsername: " << stunMessage->getUsername().value();
					break;
				case Attribute::StunPassword:
					// BCTBX_SLOGW << "\tStunPassword";
					// Ignore this deprecated attribute.
					std::ignore = decodeString(length, MAX_USERNAME_LENGTH);
					break;
				case Attribute::StunMessageIntegrity: {
					stunMessage->mMessageIntegrity = decodeMessageIntegrity(length);
					stunMessage->enableMessageIntegrity(true);
					if (stunMessage->mMessageIntegrity == StunMessage::DUMMY_MESSAGE_INTEGRITY) {
						stunMessage->enableDummyMessageIntegrity(true);
					}
					// BCTBX_SLOGW << "\tStunMessageIntegrity: " << std::hex << std::setfill('0') << std::setw(2)
					//             << static_cast<int>(stunMessage->mMessageIntegrity.value()[0])
					//             << static_cast<int>(stunMessage->mMessageIntegrity.value()[1])
					//             << static_cast<int>(stunMessage->mMessageIntegrity.value()[2])
					//             << static_cast<int>(stunMessage->mMessageIntegrity.value()[3])
					//             << static_cast<int>(stunMessage->mMessageIntegrity.value()[4])
					//             << static_cast<int>(stunMessage->mMessageIntegrity.value()[5])
					//             << static_cast<int>(stunMessage->mMessageIntegrity.value()[6])
					//             << static_cast<int>(stunMessage->mMessageIntegrity.value()[7])
					//             << static_cast<int>(stunMessage->mMessageIntegrity.value()[8])
					//             << static_cast<int>(stunMessage->mMessageIntegrity.value()[9])
					//             << static_cast<int>(stunMessage->mMessageIntegrity.value()[10])
					//             << static_cast<int>(stunMessage->mMessageIntegrity.value()[11])
					//             << static_cast<int>(stunMessage->mMessageIntegrity.value()[12])
					//             << static_cast<int>(stunMessage->mMessageIntegrity.value()[13])
					//             << static_cast<int>(stunMessage->mMessageIntegrity.value()[14])
					//             << static_cast<int>(stunMessage->mMessageIntegrity.value()[15])
					//             << static_cast<int>(stunMessage->mMessageIntegrity.value()[16])
					//             << static_cast<int>(stunMessage->mMessageIntegrity.value()[17])
					//             << static_cast<int>(stunMessage->mMessageIntegrity.value()[18])
					//             << static_cast<int>(stunMessage->mMessageIntegrity.value()[19]);
				} break;
				case Attribute::StunErrorCode:
					// BCTBX_SLOGW << "\tStunErrorCode";
					stunMessage->setError(decodeErrorCode(length));
					break;
				case Attribute::StunXorMappedAddress:
					// BCTBX_SLOGW << "\tStunXorMappedAddress";
					stunMessage->setXorMappedAddress(decodeAddress(length).toXor(stunMessage->getTransactionId()));
					break;
				case Attribute::StunSoftware:
					// BCTBX_SLOGW << "\tStunSoftware";
					stunMessage->setSoftware(decodeString(length, MAX_SOFTWARE_LENGTH));
					break;
				case Attribute::StunFingerprint: {
					std::ignore = decodeFingerprint(length);
					// const auto fingerprint = decodeFingerprint(length);
					stunMessage->enableFingerprint(true);
					// BCTBX_SLOGW << "\tStunFingerprint: " << std::hex << std::setfill('0') << std::setw(8)
					//             << fingerprint;
				} break;
				case Attribute::StunRealm:
					// BCTBX_SLOGW << "\tStunRealm";
					stunMessage->setRealm(decodeString(length, MAX_REALM_LENGTH));
					break;
				case Attribute::StunNonce:
					// BCTBX_SLOGW << "\tStunNonce";
					stunMessage->setNonce(decodeString(length, MAX_NONCE_LENGTH));
					break;
				case Attribute::TurnXorPeerAddress:
					// BCTBX_SLOGW << "\tTurnXorPeerAddress";
					stunMessage->setXorPeerAddress(decodeAddress(length).toXor(stunMessage->getTransactionId()));
					break;
				case Attribute::TurnXorRelayedAddress:
					// BCTBX_SLOGW << "\tTurnXorRelayedAddress";
					stunMessage->setXorRelayedAddress(decodeAddress(length).toXor(stunMessage->getTransactionId()));
					break;
				case Attribute::TurnLifetime:
					// BCTBX_SLOGW << "\tTurnLifetime";
					stunMessage->setLifetime(decodeLifetime(length));
					break;
				case Attribute::TurnData:
					// BCTBX_SLOGW << "\tTurnData";
					stunMessage->setData(reinterpret_cast<const char *>(decode(length)), length);
					break;
				case Attribute::IcePriority:
					stunMessage->setPriority(decodePriority(length));
					// BCTBX_SLOGW << "\tIcePriority: " << std::dec << stunMessage->getPriority().value();
					break;
				case Attribute::IceUseCandidate:
					// BCTBX_SLOGW << "\tIceUseCandidate";
					stunMessage->setUseCandidate(true);
					break;
				case Attribute::IceControlled:
					stunMessage->setIceControlled(decodeIceControlled(length));
					// BCTBX_SLOGW << "\tIceControlled: " << std::hex << std::setfill('0') << std::setw(16)
					// << stunMessage->getIceControlled().value();
					break;
				case Attribute::IceControlling:
					stunMessage->setIceControlling(decodeIceControlling(length));
					// BCTBX_SLOGW << "\tIceControlling: " << std::hex << std::setfill('0') << std::setw(16)
					//             << stunMessage->getIceControlling().value();
					break;
				default:
					if (static_cast<uint16_t>(attribute) <= 0x7FFF) {
						std::ostringstream oss;
						oss << "STUN unknown Comprehension-Required attribute: 0x" << std::hex << std::setw(4)
						    << std::setfill('0') << static_cast<uint16_t>(attribute);
						throw std::runtime_error(oss.str());
					} else {
						std::ostringstream oss;
						oss << "STUN unknown attribute: 0x" << std::hex << std::setw(4) << std::setfill('0')
						    << static_cast<uint16_t>(attribute);
						BCTBX_SLOGW << oss.str();
						std::ignore = decode(length);
					}
					break;
			}

			const auto padding = 4 - (length % 4);
			if (padding < 4) {
				for (size_t i = 0; i < padding; i++) {
					std::ignore = decode8();
				}
			}
		}

		return stunMessage;
	} catch (const std::exception &e) {
		BCTBX_SLOGE << e.what();
		return nullptr;
	}
}

std::shared_ptr<StunMessage> StunRawMessage::parseHeader() {
	const auto value = decode16();
	const auto type = static_cast<StunMessage::Type>(value & 0x0110);
	const auto method = static_cast<StunMessage::Method>(value & 0x3EEF);
	const auto length = static_cast<size_t>(decode16());
	if ((length + MESSAGE_HEADER_LENGTH) != mData.size()) {
		std::ostringstream oss;
		oss << "STUN message header length does not match message size: " << length << " - " << mData.size();
		throw std::runtime_error(oss.str());
	}
	const auto magicCookie = decode32();
	if (magicCookie != STUN_MAGIC_COOKIE) {
		throw std::runtime_error("STUN magic cookie is incorrect");
	}
	const auto transactionId = StunTransactionId(*reinterpret_cast<const UInt96 *>(decode(sizeof(UInt96))));

	std::shared_ptr<StunMessage> stunMessage = std::shared_ptr<StunMessage>(new StunMessage(type, method));
	stunMessage->setTransactionId(transactionId);
	return stunMessage;
}

void StunRawMessage::setMessageLength(const size_t length) {
	const uint16_t networkLength = htons(static_cast<uint16_t>(length));
	memcpy(&mData[STUN_MESSAGE_LENGTH_INDEX], &networkLength, sizeof(networkLength));
}

void StunRawMessage::updateMessageLength(const size_t additionalLength) {
	setMessageLength(mData.size() - MESSAGE_HEADER_LENGTH + additionalLength);
}

} // namespace ms2::nat
