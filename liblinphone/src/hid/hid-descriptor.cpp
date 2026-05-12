/*
 * Copyright (c) 2010-2026 Belledonne Communications SARL.
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

#include "hid-descriptor.h"

#include <algorithm>

#include "logger/logger.h"

LINPHONE_BEGIN_NAMESPACE

/**
 * Item types (see sections 6.2.2.1, 6.2.2.2 and 6.2.2.3 of the HID specification).
 */
enum class ItemType : uint8_t {
	Main = 0,
	Global = 1,
	Local = 2,
	Long = 3,
};

/**
 * Collection item type (see section 6.2.2.6 of the HID specification).
 */
enum class CollectionType : uint8_t {
	Physical = 0x00,
	Application = 0x01,
	Logical = 0x02,
	Report = 0x03,
	NamedArray = 0x04,
	UsageSwitch = 0x05,
	UsageModifier = 0x06,
};

/**
 * Main item tag (see section 6.2.2.4 of the HID specification).
 */
enum class MainTag : uint8_t {
	Input = 0x08,
	Output = 0x09,
	Feature = 0x0B,
	Collection = 0x0A,
	EndCollection = 0x0C,
};

/**
 * Global item tag (see section 6.2.2.7 of the HID specification).
 */
enum class GlobalTag : uint8_t {
	UsagePage = 0x00,
	LogicalMinimum = 0x01,
	LogicalMaximum = 0x02,
	PhysicalMinimum = 0x03,
	PhysicalMaximum = 0x04,
	UnitExponent = 0x05,
	Unit = 0x06,
	ReportSize = 0x07,
	ReportId = 0x08,
	ReportCount = 0x09,
	Push = 0x0A,
	Pop = 0x0B,
};

/**
 * Local item tag (see section 6.2.2.8 of the HID specification).
 */
enum class LocalTag : uint8_t {
	Usage = 0x00,
	UsageMinimum = 0x01,
	UsageMaximum = 0x02,
	DesignatorIndex = 0x03,
	DesignatorMin = 0x04,
	DesignatorMax = 0x05,
	StringIndex = 0x07,
	StringMinimum = 0x08,
	StringMaximum = 0x09,
	Delimiter = 0x0A,
};

HidReportDescriptor::HidReportDescriptor(const uint8_t *data, const size_t length)
    : mRawData(std::vector<uint8_t>(data, data + length)),
      mParsingState(ParsingState::LookingForTelephonyHeadsetApplicationCollection) {
	size_t i = 0;

	while (mParsingState != ParsingState::ParsedTelephonyHeadsetApplicationCollection && i < length) {
		const uint8_t prefix = data[i++];

		// Long item (0xFE prefix), rarely used, skipping...
		if (prefix == 0xFE) {
			if (i + 2 > length) {
				lError() << "HidDescriptor: Truncated long item";
				break;
			}
			const uint8_t dataLen = data[i++];
			i++; // Ignore long tag
			i += dataLen;
			continue;
		}

		// Short Item
		const size_t sizeCode = (prefix & 0x03);                           // bits 0 & 1
		const auto itemType = static_cast<ItemType>((prefix >> 2) & 0x03); // bits 2 & 3
		const uint8_t tag = (prefix >> 4) & 0x0F;                          // bits 4 to 7

		// Real data size
		const size_t dataLen = (sizeCode == 3) ? 4 : sizeCode; // 0, 1, 2 or 4

		if (i + dataLen > length) {
			lError() << "HidDescriptor: Truncated descriptor at offset " << i;
			break;
		}

		const uint32_t unsignedValue = readUnsigned(data + i, dataLen);
		const int32_t signedValue = readSigned(data + i, dataLen);
		i += dataLen;

		switch (itemType) {
			case ItemType::Global:
				updateGlobalState(tag, unsignedValue, signedValue);
				break;
			case ItemType::Local:
				updateLocalState(tag, unsignedValue);
				break;
			case ItemType::Main:
				handleMain(tag, unsignedValue);
				break;
			case ItemType::Long:
				// Ignore this item type
				break;
		}
	}
}

std::vector<std::shared_ptr<const HidReportDescriptor::Report>> HidReportDescriptor::getReports() const {
	return mApplicationCollection->getReports();
}

std::string HidReportDescriptor::getRawData() const {
	constexpr size_t bytesPerLine = 16;
	std::ostringstream out;

	auto length = mRawData.size();
	for (size_t i = 0; i < length; i += bytesPerLine) {
		for (size_t j = 0; j < bytesPerLine; j++) {
			if (i + j < length) {
				out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(mRawData[i + j]) << ' ';
			}
			if (j == 7) out << ' ';
		}
		out << std::endl;
	}
	return out.str();
}

void HidReportDescriptor::setParsingState(const ParsingState parsingState) {
	mParsingState = parsingState;
	if (mParsingState == ParsingState::FoundTelephonyHeadsetApplicationCollection) {
		mApplicationCollection = std::make_shared<ApplicationCollection>();
	}
}

[[nodiscard]] bool HidReportDescriptor::isUsagePage(UsagePage usagePage) const {
	return static_cast<uint32_t>(usagePage) == mGlobalState.usagePage;
}

[[nodiscard]] bool HidReportDescriptor::hasTelephonyUsage(TelephonyUsage usage) const {
	return isUsagePage(UsagePage::Telephony) && std::find(mLocalState.usages.begin(), mLocalState.usages.end(),
	                                                      static_cast<uint32_t>(usage)) != mLocalState.usages.end();
}

void HidReportDescriptor::updateGlobalState(const uint32_t tag,
                                            const uint32_t unsignedValue,
                                            const int32_t signedValue) {
	switch (static_cast<GlobalTag>(tag)) {
		case GlobalTag::UsagePage:
			mGlobalState.usagePage = unsignedValue;
			break;
		case GlobalTag::LogicalMinimum:
			mGlobalState.logicalMin = signedValue;
			break;
		case GlobalTag::LogicalMaximum:
			mGlobalState.logicalMax = signedValue;
			break;
		case GlobalTag::ReportSize:
			mGlobalState.reportSize = unsignedValue;
			break;
		case GlobalTag::ReportId:
			mGlobalState.reportID = unsignedValue;
			break;
		case GlobalTag::ReportCount:
			mGlobalState.reportCount = unsignedValue;
			break;
		case GlobalTag::PhysicalMinimum:
		case GlobalTag::PhysicalMaximum:
		case GlobalTag::UnitExponent:
		case GlobalTag::Unit:
		case GlobalTag::Push:
		case GlobalTag::Pop:
			// Ignore this tags, they are not of interest to us
			break;
	}
}

void HidReportDescriptor::updateLocalState(const uint32_t tag, const uint32_t unsignedValue) {
	switch (static_cast<LocalTag>(tag)) {
		case LocalTag::Usage:
			mLocalState.usages.push_back(unsignedValue);
			break;
		case LocalTag::UsageMinimum:
			mLocalState.usageMin = unsignedValue;
			break;
		case LocalTag::UsageMaximum:
			mLocalState.usageMax = unsignedValue;
			break;
		case LocalTag::DesignatorIndex:
		case LocalTag::DesignatorMin:
		case LocalTag::DesignatorMax:
		case LocalTag::StringIndex:
		case LocalTag::StringMinimum:
		case LocalTag::StringMaximum:
		case LocalTag::Delimiter:
			// Ignore these tags, they are not of interest to us
			break;
	}
}

void HidReportDescriptor::handleMain(const uint32_t tag, const uint32_t unsignedValue) {
	switch (const auto mainTag = static_cast<MainTag>(tag)) {
		case MainTag::Collection:
			switch (mParsingState) {
				case ParsingState::LookingForTelephonyHeadsetApplicationCollection:
					if ((static_cast<CollectionType>(unsignedValue) == CollectionType::Application) &&
					    hasTelephonyUsage(TelephonyUsage::Headset)) {
						setParsingState(ParsingState::FoundTelephonyHeadsetApplicationCollection);
					}
					break;
				case ParsingState::FoundTelephonyHeadsetApplicationCollection:
					if (static_cast<CollectionType>(unsignedValue) == CollectionType::Logical) {
						mCurrentCollection = std::make_shared<Collection>();
					}
					break;
				case ParsingState::ParsedTelephonyHeadsetApplicationCollection:
					break;
			}
			break;
		case MainTag::EndCollection:
			if (mParsingState == ParsingState::FoundTelephonyHeadsetApplicationCollection) {
				if (mCurrentCollection) {
					mApplicationCollection->addCollection(mCurrentCollection);
					mCurrentCollection.reset();
				} else {
					// Finished parsing Telephony Headset Application Collection
					setParsingState(ParsingState::ParsedTelephonyHeadsetApplicationCollection);
				}
			}
			break;
		case MainTag::Input:
		case MainTag::Output:
			if (mParsingState == ParsingState::FoundTelephonyHeadsetApplicationCollection) {
				const auto report = std::make_shared<Report>(
				    mainTag == MainTag::Input ? ReportType::Input : ReportType::Output, mGlobalState, mLocalState);
				if (mCurrentCollection) {
					mCurrentCollection->addReport(report);
				} else {
					mApplicationCollection->addReport(report);
				}
			}
			break;
		case MainTag::Feature:
			// Ignore this tag
			break;
	}

	clearLocalState();
}

void HidReportDescriptor::clearLocalState() {
	mLocalState = LocalState{};
}

int32_t HidReportDescriptor::readSigned(const uint8_t *data, const size_t size) {
	int32_t value = 0;
	for (size_t i = 0; i < size; i++) {
		value |= (data[i] << (i * 8));
	}
	// Fix sign if needed
	if ((size == 1) && (value & 0x80)) value |= 0xFFFFFF00;
	if ((size == 2) && (value & 0x8000)) value |= 0xFFFF0000;
	return value;
}

uint32_t HidReportDescriptor::readUnsigned(const uint8_t *data, const size_t size) {
	uint32_t value = 0;
	for (size_t i = 0; i < size; i++) {
		value |= (static_cast<uint32_t>(data[i]) << (i * 8));
	}
	return value;
}

LINPHONE_END_NAMESPACE
