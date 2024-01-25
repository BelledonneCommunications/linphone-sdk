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

#ifndef ms2_smff_h
#define ms2_smff_h

#include "mediastreamer-config.h"

#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "bctoolbox/vfs.h"

#ifdef HAVE_ZLIB
#include "zlib.h"
#endif

#include "../multimedia-container-interface.h"

using namespace mediastreamer::multimedia_container;
/*
 * Declarations for the Simple Multimedia File Format "prototype".
 */

namespace mediastreamer {

#ifdef HAVE_ZLIB

#ifdef _WIN32
// Disable C4251 triggered by need to export all stl template classes
#pragma warning(disable : 4251)
#endif // ifdef _WIN32

namespace SMFF {

typedef uint32_t FilePos;

/*
 *
 * SMFF file = Root | Data part | TrackDescriptor | Record... | TrackDescriptor | Records...
 */

class FileWriter;
class FileReader;
class TrackReader;

class MS2_PUBLIC Record : public RecordInterface {
	friend class FileWriter;
	friend class TrackWriter;
	friend class TrackReader;

public:
	Record() = default;
	explicit Record(const RecordInterface &ri) : RecordInterface(ri) {
	}

private:
	FilePos pos = 0;
};

class MS2_PUBLIC TrackWriter : public TrackWriterInterface {
	friend class FileWriter;

public:
	TrackWriter(const TrackWriter &) = delete;
	TrackWriter(
	    FileWriter &writer, unsigned trackID, const std::string &codec, MediaType type, int clockRate, int channels);
	virtual void addRecord(const RecordInterface &record) override;
	virtual bool empty() const override;
	TrackWriter &operator=(const TrackWriter &) = delete;

private:
	uint32_t toAbsoluteTimestamp(uint32_t trackTimestamp);
	void moveDataFromReader(TrackReader &reader);
	void adjustTimestamp(Record &rec);
	void synchronize();
	void write();
	std::vector<Record> mRecords;
	FileWriter &mFileWriter;
	int32_t mTimeOffset = 0;
	bool mSynchronized = true;
};

class MS2_PUBLIC FileWriter : public FileWriterInterface {
	friend class TrackWriter;

public:
	FileWriter();
	~FileWriter();
	FileWriter(const FileWriter &) = delete;
	FileWriter &operator=(const FileWriter &) = delete;
	virtual int open(const std::string &filename, bool append) override;
	virtual std::optional<std::reference_wrapper<TrackWriterInterface>> addTrack(unsigned trackID,
	                                                                             const std::string &codec,
	                                                                             TrackInterface::MediaType type,
	                                                                             int clockRate,
	                                                                             int channels) override;
	virtual std::optional<std::reference_wrapper<TrackWriterInterface>>
	getMainTrack(TrackInterface::MediaType type) override;
	virtual std::optional<std::reference_wrapper<TrackWriterInterface>> getTrackByID(unsigned id) override;
	virtual void synchronizeTracks() override;
	virtual int close() override;

private:
	void moveDataFromReader(FileReader &reader);
	bool write(const void *data, size_t size, off_t offset, const char *what);
	void beginCompression();
	void endCompression();
	bool write(const void *data, size_t size, const char *what);
	bool _write(const void *data, size_t size, const char *what);
	bool writeRoot();
	bool writeRecord(Record &record, uint32_t absoluteTimestamp);
	std::list<std::unique_ptr<TrackWriter>> mTrackWriters;
	FilePos mTrackPos = 0;
	FilePos mDataStartPos;
	FilePos mWritePos;
	bctbx_vfs_file_t *mFile = nullptr;
	uint32_t mMostRecentAbsTimestamp = 0;
	z_stream mZlibStream;
	bool mCompress = false;
};

class FileReader;

class MS2_PUBLIC TrackReader : public TrackReaderInterface {
	friend class FileReader;
	friend class TrackWriter;

public:
	TrackReader(FileReader &reader, unsigned id, const std::string &codec, MediaType type, int clockRate, int channels);
	TrackReader(const TrackReader &) = delete;
	TrackReader &operator=(const TrackReader &) = delete;
	void seekToTimestamp(RecordInterface::Timestamp timestamp);
	virtual bool read(RecordInterface &record) override;
	virtual bool next() override;
	virtual uint32_t getDurationMs() override;
	size_t getNumRecords() const {
		return mRecords.size();
	}

private:
	size_t seek(size_t lowIndex, size_t highIndex, RecordInterface::Timestamp timestamp);
	size_t affineSeek(size_t lowIndex, size_t highIndex, RecordInterface::Timestamp timestamp);
	bool loadRecords(uint32_t numRecords);
	FileReader &mFileReader;
	std::vector<Record> mRecords;
	size_t mCurrentRecord = 0;
};

class MS2_PUBLIC FileReader : public FileReaderInterface {
	friend class TrackReader;
	friend class FileWriter;

public:
	FileReader();
	~FileReader();
	FileReader(const FileReader &) = delete;
	FileReader &operator=(const FileReader &) = delete;
	virtual int open(const std::string &filename) override;
	virtual std::list<std::reference_wrapper<TrackReaderInterface>> getTrackReaders() override;
	virtual std::optional<std::reference_wrapper<TrackReaderInterface>>
	getMainTrack(TrackInterface::MediaType type) override;
	virtual std::optional<std::reference_wrapper<TrackReaderInterface>> getTrackByID(unsigned id) override;
	virtual void seekMilliseconds(uint32_t ms) override;
	virtual uint32_t getDurationMs() const override;
	virtual void close() override;

private:
	bool read(void *data, size_t size, off_t offset, const char *what);
	bool read(void *data, size_t size, const char *what);
	bool _read(void *data, size_t *size, const char *what, bool acceptShortRead);
	void beginDecompression();
	void endDecompression();
	bool readRoot();
	bool readTrack();
	bool readTracks();
	std::list<std::unique_ptr<TrackReader>> mTrackReaders;
	bctbx_vfs_file_t *mFile = nullptr;
	FilePos mTrackPos = 0;
	FilePos mTrackEnd;
	FilePos mDataStartPos;
	FilePos mDataEndPos;
	z_stream mZlibStream;
	std::vector<uint8_t> mZlibInputBuffer;
	bool mUncompress = false;
	bool mUncompressDone;
};

} // namespace SMFF
#endif // HAVE_ZLIB

class MS2_PUBLIC SMFFFactory {
public:
	static SMFFFactory &get();
	std::unique_ptr<FileReaderInterface> createReader();
	std::unique_ptr<FileWriterInterface> createWriter();

private:
	static SMFFFactory sInstance;
};

} // namespace mediastreamer

#endif
