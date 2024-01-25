/*
 * Copyright (c) 2024-2024 Belledonne Communications SARL.
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

#ifndef multi_media_container_interface_h
#define multi_media_container_interface_h

#include "mediastreamer2/mscommon.h"

#include <list>
#include <optional>

/* This file defines a basic, generic interface for reading and writing media data into a file container */

namespace mediastreamer {

namespace multimedia_container {

#ifdef _WIN32
// Disable C4251 triggered by need to export all stl template classes
#pragma warning(disable : 4251)
#endif // ifdef _WIN32

/**
 * Base class to represent read or write track.
 */
class MS2_PUBLIC TrackInterface {
public:
	enum class MediaType { Audio, Video };
	TrackInterface(unsigned int trackID, const std::string &codec, MediaType type, int clockRate, int channels)
	    : mCodec(codec), mType(type), mClockRate(clockRate), mChannels(channels), mTrackID(trackID) {
	}
	const std::string &getCodec() const {
		return mCodec;
	}
	MediaType getType() const {
		return mType;
	}
	int getClockRate() const {
		return mClockRate;
	}
	int getChannels() const {
		return mChannels;
	}
	unsigned getTrackID() const {
		return mTrackID;
	}
	virtual ~TrackInterface() = default;

protected:
	std::string mCodec;
	MediaType mType;
	int mClockRate;
	int mChannels;
	unsigned mTrackID;
};

/**
 * Base class to represent something to read or write from/to a track.
 */
class MS2_PUBLIC RecordInterface {
public:
	// RecordInterface(const RecordInterface &other) = default;
	using Timestamp = uint32_t;
	Timestamp timestamp = 0;
	union {
		const uint8_t *inputBuffer;
		uint8_t *outputBuffer;
	} data = {nullptr};
	size_t size = 0;
	virtual ~RecordInterface() = default;
};

/**
 * Interface to write something into a track.
 */
class MS2_PUBLIC TrackWriterInterface : public TrackInterface {
public:
	TrackWriterInterface(unsigned int trackID, const std::string &codec, MediaType type, int clockRate, int channels)
	    : TrackInterface(trackID, codec, type, clockRate, channels){};
	virtual void addRecord(const RecordInterface &record) = 0;
	virtual bool empty() const = 0;
	virtual ~TrackWriterInterface() = default;
};

/**
 * Interface to create a multimedia container from scratch or in append mode.
 * Tracks can be retrieved, added and synchronized.
 * Tracks implement the TrackWriterInterface only, which means that data from them cannot be read.
 */
class MS2_PUBLIC FileWriterInterface {
public:
	virtual int open(const std::string &filename, bool append) = 0;
	virtual std::optional<std::reference_wrapper<TrackWriterInterface>> addTrack(
	    unsigned trackID, const std::string &codec, TrackInterface::MediaType type, int clockRate, int channels) = 0;
	virtual std::optional<std::reference_wrapper<TrackWriterInterface>>
	getMainTrack(TrackInterface::MediaType type) = 0;
	virtual std::optional<std::reference_wrapper<TrackWriterInterface>> getTrackByID(unsigned id) = 0;
	/**
	 * Request tracks to be synchronized for any future addRecord() operation made on tracks.
	 */
	virtual void synchronizeTracks() = 0;
	virtual int close() = 0;
	virtual ~FileWriterInterface() = default;
};

/**
 * Interface to read from a track.
 */
class MS2_PUBLIC TrackReaderInterface : public TrackInterface {
public:
	TrackReaderInterface(unsigned int trackID, const std::string &codec, MediaType type, int clockRate, int channels)
	    : TrackInterface(trackID, codec, type, clockRate, channels){};
	virtual bool read(RecordInterface &record) = 0;
	virtual bool next() = 0;
	virtual uint32_t getDurationMs() = 0;
	virtual ~TrackReaderInterface() = default;
};

/**
 * Interface to read from a multimedia container file.
 * Tracks can be retrieved as a list of TrackReaderInterfaces.
 * It is possible to seek into the file, in which track readers are positionned to the wished position.
 */
class MS2_PUBLIC FileReaderInterface {
public:
	virtual int open(const std::string &filename) = 0;
	virtual std::list<std::reference_wrapper<TrackReaderInterface>> getTrackReaders() = 0;
	virtual std::optional<std::reference_wrapper<TrackReaderInterface>>
	getMainTrack(TrackInterface::MediaType type) = 0;
	virtual std::optional<std::reference_wrapper<TrackReaderInterface>> getTrackByID(unsigned id) = 0;
	virtual void seekMilliseconds(uint32_t ms) = 0;
	virtual uint32_t getDurationMs() const = 0;
	virtual void close() = 0;
	virtual ~FileReaderInterface() = default;
};

} // namespace multimedia_container

} // namespace mediastreamer

#endif
