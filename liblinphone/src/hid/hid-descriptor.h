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

#ifndef LINPHONE_HID_DESCRIPTOR_H
#define LINPHONE_HID_DESCRIPTOR_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "linphone/utils/general.h"

LINPHONE_BEGIN_NAMESPACE

/**
 * This class represents and parses an HID Report Descriptor as described in the HID specification: "Universal Serial
 * Bus (USB) – Device Class Definition for Human Interface Devices (HID) – Version 1.11". Mainly from the Section 6.2.2.
 *
 * It does not parse the full HID Report Descriptor but focuses on the Application Collection with a Telephony Usage
 * Page and Headset Usage.
 */
class HidReportDescriptor {
public:
	/**
	 * Store the global state during the parsing of an HID report descriptor (see section 5.4 of the HID specification
	 * for details on the parsing mechanism).
	 */
	struct GlobalState {
		uint32_t usagePage = 0;
		int32_t logicalMin = 0;
		int32_t logicalMax = 0;
		uint32_t reportID = 0;
		uint32_t reportSize = 0;
		uint32_t reportCount = 0;
	};

	/**
	 * Store the local state during the parsing of an HID report descriptor (see section 5.4 of the HID specification
	 * for details on the parsing mechanism).
	 */
	struct LocalState {
		std::vector<uint32_t> usages{};
		uint32_t usageMin = 0;
		uint32_t usageMax = 0;
	};

	/**
	 * Non-specified state that we use to limit the parsing to the Telephony Usage Page and Headset Usage.
	 */
	enum class ParsingState : uint8_t {
		LookingForTelephonyHeadsetApplicationCollection = 0,
		FoundTelephonyHeadsetApplicationCollection = 1,
		ParsedTelephonyHeadsetApplicationCollection = 2,
	};

	/**
	 * Definition of the HID Usage Pages that are relevant to us (see the "HID Usage Tables for Universal Serial Bus
	 * (USB) – Version 1.7" specification).
	 */
	enum class UsagePage : uint8_t {
		LEDs = 0x08,
		Button = 0x09,
		Telephony = 0x0B,
		Consumer = 0x0C,
	};

	/**
	 * Definition of the HID LED Page Usages that are relevant to us (see the "HID Usage Tables for Universal Serial Bus
	 * (USB) – Version 1.7" specification).
	 */
	enum class LedsUsage : uint8_t {
		Mute = 0x09,
		OffHook = 0x17,
		Ring = 0x18,
		Hold = 0x20,
		Microphone = 0x21,
		OnLine = 0x2A,
	};

	/**
	 * Definition of the HID Telephony Device Page Usages that are relevant to us (see the "HID Usage Tables for
	 * Universal Serial Bus (USB) – Version 1.7" specification).
	 */
	enum class TelephonyUsage : uint8_t {
		Headset = 0x05,
		TelephonyKeyPad = 0x06,
		ProgrammableButton = 0x07,
		HookSwitch = 0x20,
		Flash = 0x21,
		Redial = 0x24,
		Line = 0x2A,
		PhoneMute = 0x2F,
		SpeedDial = 0x50,
		LineBusyTone = 0x97,
		Ringer = 0x9E,
	};

	/**
	 * Report item Type (see section 6.2.2.5 of the HID specification).
	 */
	enum class ReportType : uint8_t {
		Input = 0x00,
		Output = 0x01,
		Feature = 0x02,
	};

	class Report;

	/**
	 * Base class for the Report and Collection items.
	 */
	class Item : public std::enable_shared_from_this<Item> {
	public:
		Item() = default;
		virtual ~Item() = default;

		[[nodiscard]] virtual std::vector<std::shared_ptr<const Report>> getReports() const = 0;
	};

	/**
	 * Report item that can be contained in a Collection or directly in an ApplicationCollection (see section 5.4 of the
	 * HID specification for a figure of the relations between ApplicationCollection, Collection, and Report).
	 */
	class Report : public Item {
	public:
		Report(const ReportType reportType, const GlobalState &globalState, const LocalState &localState)
		    : mReportType(reportType), mUsagePage(globalState.usagePage), mReportID(globalState.reportID),
		      mReportSize(globalState.reportSize), mReportCount(globalState.reportCount), mUsages(localState.usages) {
		}
		~Report() override = default;

		[[nodiscard]] ReportType getReportType() const {
			return mReportType;
		}
		[[nodiscard]] uint32_t getReportID() const {
			return mReportID;
		}
		[[nodiscard]] uint32_t getReportSize() const {
			return mReportSize;
		}
		[[nodiscard]] uint32_t getReportCount() const {
			return mReportCount;
		}
		[[nodiscard]] uint32_t getUsagePage() const {
			return mUsagePage;
		}
		[[nodiscard]] const std::vector<uint32_t> &getUsages() const {
			return mUsages;
		}

		std::vector<std::shared_ptr<const Report>> getReports() const override {
			std::vector<std::shared_ptr<const Report>> reports;
			reports.push_back(std::static_pointer_cast<const Report>(shared_from_this()));
			return reports;
		}

	private:
		ReportType mReportType;
		uint32_t mUsagePage;
		uint32_t mReportID;
		uint32_t mReportSize;
		uint32_t mReportCount;
		std::vector<uint32_t> mUsages;
	};

	/**
	 * Collection item that can contain Report items (see section 5.4 of the HID specification for a figure of the
	 * relations between ApplicationCollection, Collection, and Report).
	 */
	class Collection : public Item {
	public:
		Collection() = default;
		~Collection() override = default;

		void addReport(const std::shared_ptr<Report> &report) {
			mReports.push_back(report);
		}

		[[nodiscard]] std::vector<std::shared_ptr<const Report>> getReports() const override {
			return mReports;
		}

	private:
		std::vector<std::shared_ptr<const Report>> mReports;
	};

	/**
	 * Application Collection item that can contain Report and Collection items (see section 5.4 of the HID
	 * specification for a figure of the relations between ApplicationCollection, Collection, and Report)
	 */
	class ApplicationCollection {
	public:
		ApplicationCollection() = default;
		~ApplicationCollection() = default;

		void addCollection(const std::shared_ptr<const Collection> &collection) {
			mItems.push_back(collection);
		}

		void addReport(const std::shared_ptr<const Report> &report) {
			mItems.push_back(report);
		}

		[[nodiscard]] std::vector<std::shared_ptr<const Report>> getReports() const {
			std::vector<std::shared_ptr<const Report>> reports;

			for (auto &item : mItems) {
				auto r = item->getReports();
				for (auto &report : item->getReports()) {
					reports.push_back(report);
				}
			}

			return reports;
		}

	private:
		std::vector<std::shared_ptr<const Item>> mItems;
	};

	HidReportDescriptor(const uint8_t *data, size_t length);
	~HidReportDescriptor() = default;

	/**
	 * Get an ordered flattened list of all the reports contained in the Collection and Report items from the
	 * Application Collection with a Telephony Usage Page and Heaset Usage.
	 */
	[[nodiscard]] std::vector<std::shared_ptr<const Report>> getReports() const;

	/**
	 * Get the full raw data of the HID report descriptor.
	 */
	[[nodiscard]] std::string getRawData() const;

private:
	void setParsingState(ParsingState parsingState);

	[[nodiscard]] bool isUsagePage(UsagePage usagePage) const;
	[[nodiscard]] bool hasTelephonyUsage(TelephonyUsage usage) const;

	void updateGlobalState(uint32_t tag, uint32_t unsignedValue, int32_t signedValue);
	void updateLocalState(uint32_t tag, uint32_t unsignedValue);
	void handleMain(uint32_t tag, uint32_t unsignedValue);
	void clearLocalState();

	static int32_t readSigned(const uint8_t *data, size_t size);
	static uint32_t readUnsigned(const uint8_t *data, size_t size);

	std::vector<uint8_t> mRawData;
	GlobalState mGlobalState;
	LocalState mLocalState;
	ParsingState mParsingState;
	std::shared_ptr<ApplicationCollection> mApplicationCollection;
	std::shared_ptr<Collection> mCurrentCollection;
};
LINPHONE_END_NAMESPACE

#endif // LINPHONE_HID_DESCRIPTOR_H
