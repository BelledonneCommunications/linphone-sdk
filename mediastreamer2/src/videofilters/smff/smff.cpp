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

#include "smff.h"
#include "bctoolbox/logging.h"

#include <memory>

using namespace std;

namespace mediastreamer {

#ifdef HAVE_ZLIB
namespace SMFF {

struct SMFFRoot {
	char magic[4] = {'S', 'M', 'F', 'F'};
	uint32_t version = 0;
	FilePos trackPosition = 0;
	FilePos dataPosition = 0;
};

struct SMFFTrackDescriptor {
	char codecName[16];
	uint8_t type;
	uint8_t channels;
	uint8_t trackID;
	uint8_t unused;
	uint32_t clockrate;
	uint32_t recordsCount;
};

struct SMFFRecord {
	uint32_t timestamp;
	FilePos position; // the position inside the data part
	uint32_t size;
};

uint32_t TrackWriter::toAbsoluteTimestamp(uint32_t trackTimestamp) {
	return (uint32_t)(((uint64_t)trackTimestamp * 1000ULL) / mClockRate);
}

/*
 * FileWriter class.
 */

FileWriter::FileWriter() {
}

FileWriter::~FileWriter() {
	if (mFile) close();
}

int FileWriter::open(const std::string &filepath, bool append) {
	if (mFile != nullptr) {
		bctbx_error("FileWriter::open(): could not open(), there's already an open file.");
		return -1;
	}
	mTrackPos = 0;
	mDataStartPos = mWritePos = sizeof(SMFFRoot);
	if (append && bctbx_file_exist(filepath.c_str()) == 0) {
		FileReader reader;
		if (reader.open(filepath) != 0) {
			bctbx_error("FileWriter::open(): could not open for append, the existing file could not be loaded.");
			return -1;
		}
		moveDataFromReader(reader);
	} else append = false;

	mFile =
	    bctbx_file_open2(bctbx_vfs_get_default(), filepath.c_str(), append ? O_WRONLY : (O_WRONLY | O_TRUNC | O_CREAT));
	if (!mFile) return -1;

	if (append) {
		/* get rid of track descriptors and records, they will be rewritten at the end at close() time. */
		if (bctbx_file_truncate(mFile, (int64_t)mWritePos) != 0) {
			bctbx_error("FileWriter::open(): bctbx_file_truncate() failed");
			goto error;
		}
		if (bctbx_file_seek(mFile, (off_t)mWritePos, SEEK_SET) == BCTBX_VFS_ERROR) {
			bctbx_error("FileWriter::open(): bctbx_file_seek() failed");
			goto error;
		}
	} else {
		writeRoot();
	}
	return 0;
error:
	bctbx_file_close(mFile);
	return -1;
}

void FileWriter::moveDataFromReader(FileReader &reader) {
	mMostRecentAbsTimestamp = 0;
	for (TrackReaderInterface &track : reader.getTrackReaders()) {
		auto writeTrack =
		    addTrack(track.getTrackID(), track.getCodec(), track.getType(), track.getClockRate(), track.getChannels());
		if (writeTrack) {
			TrackWriter &tw = dynamic_cast<TrackWriter &>(writeTrack.value().get());
			tw.moveDataFromReader(dynamic_cast<TrackReader &>(track));
			if (!tw.mRecords.empty()) {
				uint32_t absoluteTimestamp = tw.toAbsoluteTimestamp(tw.mRecords.back().timestamp);
				if (absoluteTimestamp > mMostRecentAbsTimestamp) {
					mMostRecentAbsTimestamp = absoluteTimestamp;
				}
			}
		}
	}
	mWritePos = reader.mDataEndPos;
}

std::optional<std::reference_wrapper<TrackWriterInterface>> FileWriter::addTrack(
    unsigned id, const std::string &codec, TrackInterface::MediaType type, int clockRate, int channels) {
	auto writer = new TrackWriter(*this, id, codec, type, clockRate, channels);
	if (getTrackByID(id) != nullopt) {
		bctbx_error("FileWriter::addTrack() there is already a track with ID [%u]", id);
		return nullopt;
	}
	mTrackWriters.push_back(unique_ptr<TrackWriter>(writer));
	bctbx_message("FileWriter::addTrack() with id %u, codec %s, type %i, clockrate %i, channels %i", id, codec.c_str(),
	              (int)type, clockRate, channels);
	return *writer;
}

bool FileWriter::write(const void *data, size_t size, off_t offset, const char *what) {
	ssize_t ret = bctbx_file_write(mFile, data, size, offset);
	// bctbx_message("Wrote %i bytes at position %i", (int)size, (int)offset);
	if (ret == (ssize_t)size) return true;
	else if (ret == BCTBX_VFS_ERROR) {
		bctbx_error("FileWriter: error writing [%s]", what);
	} else {
		bctbx_error("FileWriter: short write while writing [%s]: only [%llu] over [%llu] bytes", what,
		            (unsigned long long)ret, (unsigned long long)size);
	}
	return false;
}

void FileWriter::beginCompression() {
	mZlibStream = {};
	int ret = deflateInit(&mZlibStream, Z_DEFAULT_COMPRESSION);
	if (ret != Z_OK) {
		bctbx_error("FileWriter::beginCompression(): deflateInit() failed.");
	}
	mCompress = true;
}

void FileWriter::endCompression() {
	write(nullptr, 0, "Flush compressed data");
	deflateEnd(&mZlibStream);
	mCompress = false;
}

bool FileWriter::write(const void *data, size_t size, const char *what) {
	if (mCompress) {
		int zret;
		bool ret = true;
		mZlibStream.avail_in = (uInt)size;
		mZlibStream.next_in = (Bytef *)data;
		do {
			uint8_t outputBuffer[128] = {};
			mZlibStream.next_out = outputBuffer;
			mZlibStream.avail_out = sizeof(outputBuffer);
			zret = deflate(&mZlibStream, size > 0 ? Z_NO_FLUSH : Z_FINISH);
			if (zret == Z_OK || zret == Z_STREAM_END) {
				size_t written = sizeof(outputBuffer) - mZlibStream.avail_out;
				if (written > 0) {
					// bctbx_message("Wrote %i bytes of compressed data at pos %i", (int) written, (int) mWritePos);
					ret = _write(outputBuffer, written, what) && ret;
				}
			} else {
				bctbx_error("FileWriter::write(): deflate error");
			}
		} while (zret == Z_OK && (mZlibStream.avail_out == 0 || mZlibStream.avail_in > 0));
		return ret;
	}
	return _write(data, size, what);
}

bool FileWriter::_write(const void *data, size_t size, const char *what) {
	if (write(data, size, mWritePos, what)) {
		mWritePos += (FilePos)size;
		return true;
	}
	return false;
}

bool FileWriter::writeRoot() {
	SMFFRoot root;
	root.trackPosition = htonl(mTrackPos);
	root.dataPosition = htonl(mDataStartPos);
	return write(&root, sizeof(root), 0, "root");
}

bool FileWriter::writeRecord(Record &record, uint32_t absoluteTimestamp) {
	/* update the last absolute timestamp for the file, which is required for offset computation and track
	 * synchronisation */
	if (absoluteTimestamp > mMostRecentAbsTimestamp) {
		mMostRecentAbsTimestamp = absoluteTimestamp;
	}
	if (write(record.data.inputBuffer, record.size, mWritePos, "data")) {
		record.pos = mWritePos;
		mWritePos += (FilePos)record.size;
		return true;
	}
	return false;
}

void FileWriter::synchronizeTracks() {
	bool allTracksEmpty = true;
	for (auto &t : mTrackWriters) {
		t->synchronize();
		if (!t->empty()) allTracksEmpty = false;
	}
	if (!allTracksEmpty) {
		/* Add a fixed time interval, so that the end of the past record portion is not the beginning of the new one*/
		mMostRecentAbsTimestamp += 100; // 100 milliseconds - arbitrary.
	}
}

std::optional<std::reference_wrapper<TrackWriterInterface>> FileWriter::getTrackByID(unsigned id) {
	for (auto &t : mTrackWriters) {
		if (t->getTrackID() == id) return *t;
	}
	return nullopt;
}

std::optional<std::reference_wrapper<TrackWriterInterface>> FileWriter::getMainTrack(TrackInterface::MediaType type) {
	for (auto &t : mTrackWriters) {
		if (t->getType() == type) return *t;
	}
	return std::nullopt;
}

int FileWriter::close() {
	if (mFile == nullptr) return -1;
	mTrackPos = mWritePos;
	beginCompression();
	for (auto &trackWriter : mTrackWriters) {
		trackWriter->write();
	}
	endCompression();
	writeRoot();
	bctbx_file_close(mFile);
	mTrackWriters.clear();
	mFile = nullptr;
	return 0;
}

/*
 * TrackWriter class.
 */

TrackWriter::TrackWriter(
    FileWriter &writer, unsigned id, const std::string &codec, MediaType type, int clockRate, int channels)
    : TrackWriterInterface(id, codec, type, clockRate, channels), mFileWriter(writer) {
}

void TrackWriter::synchronize() {
	mSynchronized = false;
}

void TrackWriter::adjustTimestamp(Record &rec) {
	if (!mSynchronized) {
		uint32_t mostRecentInTrackUnits =
		    (uint32_t)(((uint64_t)mFileWriter.mMostRecentAbsTimestamp * mClockRate) / 1000ULL);
		mTimeOffset = mostRecentInTrackUnits - rec.timestamp;
		mSynchronized = true;
	}
	rec.timestamp += mTimeOffset;
}

void TrackWriter::addRecord(const RecordInterface &record) {
	mRecords.emplace_back(Record(record));
	Record &copy = mRecords.back();
	adjustTimestamp(copy);
	uint32_t absTimestamp = toAbsoluteTimestamp(copy.timestamp);
	mFileWriter.writeRecord(copy, absTimestamp);
	copy.data.inputBuffer = nullptr; /* don't point to user memory that is not retained. */
	/*bctbx_message("TrackWriter[%p, type=%s]: adding record with raw-ts=[%u] adjusted-ts=[%u]; abs-ts=[%u]",
	              this, (getType() == multimedia_container::TrackInterface::MediaType::Audio) ? "audio" : "video",
	              record.timestamp, copy.timestamp, absTimestamp);
	*/
}

bool TrackWriter::empty() const {
	return mRecords.empty();
}

void TrackWriter::write() {
	bctbx_message("TrackWriter::write(): writing track with codec %s", getCodec().c_str());
	SMFFTrackDescriptor trackDescriptor{};
	strncpy(trackDescriptor.codecName, getCodec().c_str(), sizeof(trackDescriptor.codecName) - 1);
	trackDescriptor.channels = (uint8_t)getChannels();
	trackDescriptor.clockrate = htonl(getClockRate());
	trackDescriptor.trackID = getTrackID();
	trackDescriptor.type = (uint8_t)getType();
	trackDescriptor.recordsCount = htonl((unsigned int)mRecords.size());
	mFileWriter.write(&trackDescriptor, sizeof(trackDescriptor), "track descriptor");

	for (auto &record : mRecords) {
		SMFFRecord smffrec{};
		smffrec.position = htonl(record.pos - mFileWriter.mDataStartPos);
		smffrec.timestamp = htonl(record.timestamp);
		smffrec.size = htonl((unsigned int)record.size);
		mFileWriter.write(&smffrec, sizeof(smffrec), "record descriptor");
	}
}

void TrackWriter::moveDataFromReader(TrackReader &reader) {
	std::swap(mRecords, reader.mRecords);
}

/*
 * TrackReader class
 */

TrackReader::TrackReader(
    FileReader &reader, unsigned id, const std::string &codec, MediaType type, int clockRate, int channels)
    : TrackReaderInterface(id, codec, type, clockRate, channels), mFileReader(reader) {
	// bctbx_message("TrackerReader(): codec=[%s], clockRate=[%i]", codec.c_str(), clockRate);
}

bool TrackReader::loadRecords(uint32_t numRecords) {
	uint32_t i;
	// bctbx_message("TrackReader::loadRecords() loading [%i] records", numRecords);
	for (i = 0; i < numRecords; ++i) {
		SMFFRecord smffrec{};
		if (!mFileReader.read(&smffrec, sizeof(smffrec), "record")) {
			return false;
		}
		Record record{};
		record.pos = ntohl(smffrec.position) + mFileReader.mDataStartPos;
		record.size = ntohl(smffrec.size);
		record.timestamp = ntohl(smffrec.timestamp);
		if (record.pos < mFileReader.mDataStartPos || (record.pos + record.size) > mFileReader.mDataEndPos) {
			bctbx_error("TrackReader: Record points to outside of data segment, at index [%i]", i);
			return false;
		}
		mRecords.emplace_back(record);
	}
	return true;
}

bool TrackReader::read(RecordInterface &requestedRecord) {
	if (mCurrentRecord >= mRecords.size()) return false;
	Record &record = mRecords[mCurrentRecord];
	// bctbx_message("Current record has timestamp %u", record.timestamp);
	if (requestedRecord.timestamp >= record.timestamp) {
		size_t requestedSize = requestedRecord.size;
		requestedRecord.timestamp = record.timestamp;
		if (requestedSize > 0) {
			requestedRecord.size = MIN(record.size, requestedSize);
		} else requestedRecord.size = record.size;
		if (requestedRecord.data.outputBuffer != nullptr && requestedSize > 0) {
			return mFileReader.read(requestedRecord.data.outputBuffer, requestedRecord.size, record.pos,
			                        "data for record");
		}
		return true;
	}
	return false;
}

bool TrackReader::next() {
	if (mCurrentRecord + 1 < mRecords.size()) {
		mCurrentRecord++;
		return true;
	}
	return false;
}
size_t TrackReader::affineSeek(size_t lowIndex, size_t highIndex, RecordInterface::Timestamp timestamp) {
	size_t foundIndex;
	/* last step of the search, look for the index that shows the found timestamp */
	if (mRecords[highIndex].timestamp <= timestamp) foundIndex = highIndex;
	else foundIndex = lowIndex;
	timestamp = mRecords[foundIndex].timestamp;

	while (foundIndex > 0 && mRecords[foundIndex - 1].timestamp == timestamp)
		foundIndex--;

	return foundIndex;
}

size_t TrackReader::seek(size_t lowIndex, size_t highIndex, RecordInterface::Timestamp timestamp) {
	if (lowIndex == highIndex || lowIndex + 1 == highIndex) {
		return affineSeek(lowIndex, highIndex, timestamp);
	}
	bctbx_message("TrackReader: lookup at indexes [%u - %u] for timestamp [%u]", (unsigned)lowIndex,
	              (unsigned)highIndex, (unsigned)timestamp);

	size_t middle = (lowIndex + highIndex) / 2;
	// bctbx_message("Trackreader: middle=[%u], value [%u]", (unsigned)middle, (unsigned)mRecords[middle].timestamp);
	if (mRecords[middle].timestamp <= timestamp) {
		return seek(middle, highIndex, timestamp);
	} else {
		return seek(lowIndex, middle > 0 ? middle - 1 : 0, timestamp);
	}
}

void TrackReader::seekToTimestamp(RecordInterface::Timestamp timestamp) {
	/*
	 * Launch a search by dicotomia in order to find the first index where reseached timestamp appears.
	 * The researched timestamp might not exist, in which case the first index of the record with a timestamp appearing
	 * just before the researched timestamp is returned.
	 * Keep in mind that records have are sorted by increasing timestamp, but there might be suites of equal timestamps.
	 * ex: 200, 300, 400, 400, 400, 500, 600 etc.
	 */
	mCurrentRecord = seek(0, mRecords.size(), timestamp);
	bctbx_message("TrackReader: seek at index [%u] for timestamp [%u]", (unsigned)mCurrentRecord, (unsigned)timestamp);
}

uint32_t TrackReader::getDurationMs() {
	if (mRecords.empty()) return 0;
	return (uint32_t)(((uint64_t)mRecords.back().timestamp * 1000ULL) / (uint64_t)mClockRate);
}

/*
 * FileReader class.
 */

FileReader::FileReader() {
}

FileReader::~FileReader() {
	if (mFile) close();
}

void FileReader::close() {
	if (mFile) {
		bctbx_file_close(mFile);
		mFile = nullptr;
	}
	mTrackReaders.clear();
}

int FileReader::open(const std::string &filepath) {
	if (mFile) {
		bctbx_error("FileReader::open(): there is already an open file !");
		return -1;
	}
	mFile = bctbx_file_open(bctbx_vfs_get_default(), filepath.c_str(), "r");
	if (!mFile) return -1;
	if (!readRoot()) return -1;
	if (!readTracks()) return -1;
	return 0;
}

std::list<std::reference_wrapper<TrackReaderInterface>> FileReader::getTrackReaders() {
	std::list<std::reference_wrapper<TrackReaderInterface>> trackReaders;
	for (auto &tr : mTrackReaders) {
		trackReaders.push_back(*tr);
	}
	return trackReaders;
}

bool FileReader::read(void *data, size_t size, off_t offset, const char *what) {
	ssize_t ret = bctbx_file_read(mFile, data, size, offset);
	if (ret == (ssize_t)size) {
		return true;
	} else if (ret == BCTBX_VFS_ERROR) {
		bctbx_error("FileReader: error reading [%i] bytes for [%s]", (int)size, what);
	} else {
		bctbx_error("FileReader: short read of [%i] bytes over [%i] for [%s]", (int)ret, (int)size, what);
	}
	return false;
}

bool FileReader::read(void *data, size_t size, const char *what) {
	if (mUncompress) {
		int zret;
		mZlibStream.next_out = (Bytef *)data;
		mZlibStream.avail_out = (uInt)size;
		do {
			if (mZlibStream.avail_in == 0) {
				mZlibInputBuffer.resize(256);
				size_t attemptToRead = mZlibInputBuffer.size();
				if (!_read(mZlibInputBuffer.data(), &attemptToRead, what, true)) {
					return false;
				}
				// bctbx_message("FileReader: got %i bytes of compressed data", (int)attemptToRead);
				mZlibStream.next_in = (Bytef *)mZlibInputBuffer.data();
				mZlibStream.avail_in = (uInt)attemptToRead;
			}
			zret = inflate(&mZlibStream, Z_SYNC_FLUSH);
			if (zret == Z_OK || zret == Z_STREAM_END) {
				if (mZlibStream.avail_out == 0) {
					if (zret == Z_STREAM_END) {
						bctbx_message("FileReader: end of compressed data.");
						mUncompressDone = true;
					}
					// bctbx_message("FileReader: read %i bytes from compressed data.", (int)size);
					return true;
				}
			} else {
				bctbx_error("FileReader: inflate error: %i", zret);
			}
		} while (zret == Z_OK && mZlibStream.avail_in == 0);
		return false;
	}
	return _read(data, &size, what, false);
}

bool FileReader::_read(void *data, size_t *size, const char *what, bool acceptShortRead) {
	ssize_t ret = bctbx_file_read2(mFile, data, *size);
	if (ret == (ssize_t)*size) {
		return true;
	} else if (ret == BCTBX_VFS_ERROR) {
		bctbx_error("FileReader: error reading [%i] bytes for [%s]", (int)*size, what);
	} else {
		if (acceptShortRead) {
			*size = (size_t)ret;
			return true;
		}
		bctbx_error("FileReader: short read of [%i] bytes over [%i] for [%s]", (int)ret, (int)*size, what);
		*size = (size_t)ret;
	}
	return false;
}

void FileReader::beginDecompression() {
	mUncompress = true;
	mUncompressDone = false;
	mZlibStream = {};
	inflateInit(&mZlibStream);
}

void FileReader::endDecompression() {
	inflateEnd(&mZlibStream);
	mUncompress = false;
}

bool FileReader::readRoot() {
	SMFFRoot root;
	ssize_t fileSize;
	if (!read(&root, sizeof(root), 0, "root")) return false;
	if (strncmp(root.magic, "SMFF", sizeof(root.magic)) != 0) {
		bctbx_error("FileReader: bad magic identifier.");
		return false;
	}
	fileSize = bctbx_file_size(mFile);
	mDataStartPos = ntohl(root.dataPosition);
	if ((ssize_t)mDataStartPos > fileSize) {
		bctbx_error("FileReader: data segment starts beyond the end of file.");
		return false;
	}
	mTrackPos = ntohl(root.trackPosition);
	if ((ssize_t)mTrackPos > fileSize) {
		bctbx_error("FileReader: tracks segment starts beyond the end of file.");
		return false;
	}
	mDataEndPos = mTrackPos;
	mTrackEnd = (FilePos)fileSize;
	// bctbx_message("FileReader: read root succesfully, mDataStartPos=%i, mTrackPos=%i", mDataStartPos, mTrackEnd);
	return true;
}

bool FileReader::readTrack() {
	SMFFTrackDescriptor trackDescriptor{};
	if (!read(&trackDescriptor, sizeof(trackDescriptor), "track descriptor")) {
		return false;
	}
	trackDescriptor.codecName[sizeof(trackDescriptor.codecName) - 1] = '\0';
	auto trackReader = new TrackReader(*this, trackDescriptor.trackID, trackDescriptor.codecName,
	                                   (TrackInterface::MediaType)trackDescriptor.type,
	                                   ntohl(trackDescriptor.clockrate), trackDescriptor.channels);
	if (!trackReader->loadRecords(ntohl(trackDescriptor.recordsCount))) {
		bctbx_error("FileReader: track with codec=[%s] could not be loaded.", trackReader->getCodec().c_str());
		delete trackReader;
		return false;
	}
	bctbx_message("FileReader::readTrack(): got track with id %u, codec %s, type %i, clockrate %i",
	              trackReader->getTrackID(), trackReader->getCodec().c_str(), (int)trackReader->getType(),
	              trackReader->getClockRate());
	mTrackReaders.push_back(std::unique_ptr<TrackReader>(trackReader));
	return true;
}

bool FileReader::readTracks() {
	bool ret = true;
	bctbx_file_seek(mFile, mTrackPos, SEEK_SET);
	beginDecompression();
	// while (mFile->offset + sizeof(SMFFTrackDescriptor) < mTrackEnd) //< was without compression.
	while (!mUncompressDone) {
		if (!readTrack()) {
			ret = false;
			break;
		}
	}
	endDecompression();
	return ret;
}

std::optional<std::reference_wrapper<TrackReaderInterface>> FileReader::getMainTrack(TrackInterface::MediaType type) {
	for (auto &t : mTrackReaders) {
		if (t->getType() == type) return *t;
	}
	return std::nullopt;
}

void FileReader::seekMilliseconds(uint32_t ms) {
	for (auto &t : mTrackReaders) {
		RecordInterface::Timestamp ts = (ms * t->getClockRate()) / 1000;
		t->seekToTimestamp(ts);
	}
}

uint32_t FileReader::getDurationMs() const {
	uint32_t maxDuration = 0;
	for (auto &t : mTrackReaders) {
		uint32_t trDuration = t->getDurationMs();
		if (trDuration > maxDuration) maxDuration = trDuration;
	}
	return maxDuration;
}

std::optional<std::reference_wrapper<TrackReaderInterface>> FileReader::getTrackByID(unsigned int id) {
	for (auto &t : mTrackReaders) {
		if (t->getTrackID() == id) return *t;
	}
	return nullopt;
}

} // namespace SMFF
#endif // HAVE_ZLIB

SMFFFactory SMFFFactory::sInstance;

SMFFFactory &SMFFFactory::get() {
	return sInstance;
}

std::unique_ptr<FileReaderInterface> SMFFFactory::createReader() {
#ifdef HAVE_ZLIB
	return std::unique_ptr<FileReaderInterface>(new SMFF::FileReader());
#else
	bctbx_warning("No SMFF reader support, not compiled.");
	return nullptr;
#endif
}

std::unique_ptr<FileWriterInterface> SMFFFactory::createWriter() {
#ifdef HAVE_ZLIB
	return std::unique_ptr<FileWriterInterface>(new SMFF::FileWriter());
#else
	bctbx_warning("No SMFF reader support, not compiled.");
	return nullptr;
#endif
}

} // namespace mediastreamer
