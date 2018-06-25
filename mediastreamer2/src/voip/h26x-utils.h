/*
 Mediastreamer2 h26x-utils.h
 Copyright (C) 2018 Belledonne Communications SARL

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#pragma once

#include <cstdint>
#include <list>
#include <map>
#include <vector>

#include "mediastreamer2/msqueue.h"

namespace mediastreamer {


class H26xNaluType {
public:
	H26xNaluType() = default;
	H26xNaluType(uint8_t value);
	virtual ~H26xNaluType() = default;

	operator uint8_t() const {return _value;}

	virtual bool isVcl() const = 0;
	virtual bool isParameterSet() const = 0;
	virtual bool isKeyFramePart() const = 0;

protected:
	uint8_t _value = 0;
};

class H26xNaluHeader {
public:
	H26xNaluHeader() = default;
	virtual ~H26xNaluHeader() = default;

	void setFBit(bool val) {_fBit = val;}
	bool getFBit() const {return _fBit;}

	virtual const H26xNaluType &getAbsType() const = 0;

	virtual void parse(const uint8_t *header) = 0;
	virtual mblk_t *forge() const = 0;

	static H26xNaluHeader *createFromMime(const std::string &mime);

protected:
	bool _fBit = false;
};

class H26xUtils {
public:
	H26xUtils() = delete;

	static void naluStreamToNalus(const std::vector<uint8_t> &byteStream, MSQueue *out);
	static void naluStreamToNalus(const uint8_t *byteStream, size_t size, MSQueue *out);

	static void byteStreamToNalus(const std::vector<uint8_t> &byteStream, MSQueue *out);
	static void byteStreamToNalus(const uint8_t *byteStream, size_t size, MSQueue *out);

	static void nalusToByteStream(MSQueue *nalus, std::vector<uint8_t> &bytestream);
	static void nalusToByteStream(std::list<mblk_t *> &nalus, std::vector<uint8_t> &bytestream);
};


class H26xParameterSetsInserter {
public:
	virtual ~H26xParameterSetsInserter() = default;
	virtual void process(MSQueue *in, MSQueue *out) = 0;
	virtual void flush() = 0;

protected:
	static void replaceParameterSet(mblk_t *&ps, mblk_t *newPs);
};

class H26xParameterSetsStore {
public:
	class IlegalStateError: std::logic_error {
	public:
		IlegalStateError(const std::string &what_arg): std::logic_error(what_arg) {}
	};

	H26xParameterSetsStore(const std::initializer_list<int> &psCodes);
	virtual ~H26xParameterSetsStore();

	void extractAllPs(MSQueue *frame);
	bool psGatheringCompleted() const;
	void fetchAllPs(MSQueue *outq);

protected:
	void addPs(int naluType, mblk_t *nalu);
	virtual int getNaluType(const mblk_t *nalu) const = 0;

	std::map<int, mblk_t *> _ps;
};

}
