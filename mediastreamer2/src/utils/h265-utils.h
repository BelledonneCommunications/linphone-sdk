/*
 mediastreamer2 library - modular sound and video processing and streaming
 Copyright (C) 2015  Belledonne Communications <info@belledonne-communications.com>

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
#include <ortp/str_utils.h>

#include "h26x-utils.h"

namespace mediastreamer {

class H265NaluType {
public:
	H265NaluType() = default;
	H265NaluType(uint8_t value);

	operator uint8_t() const {return _value;}

	bool isVcl() const {return _value < 32;}
	bool isParameterSet() const;

	static const H265NaluType IdrWRadl;
	static const H265NaluType IdrNLp;
	static const H265NaluType Vps;
	static const H265NaluType Sps;
	static const H265NaluType Pps;
	static const H265NaluType Ap;
	static const H265NaluType Fu;

private:
	uint8_t _value = 0;
};

class H265NaluHeader {
public:
	H265NaluHeader() = default;
	H265NaluHeader(const uint8_t *header) {parse(header);}

	void setFBit(bool val) {_fBit = val;}
	bool getFBit() const {return _fBit;}

	void setType(H265NaluType type) {_type = type;}
	H265NaluType getType() const {return _type;}

	void setLayerId(uint8_t layerId);
	uint8_t getLayerId() const {return _layerId;}

	void setTid(uint8_t tid);
	uint8_t getTid() const {return _tid;}

	void parse(const uint8_t *header);
	mblk_t *forge() const;

	static const size_t length = 2;

private:
	bool _fBit = false;
	H265NaluType _type;
	uint8_t _layerId = 0;
	uint8_t _tid = 0;
};

class H265FuHeader {
public:
	enum class Position {
		Start,
		Middle,
		End
	};

	H265FuHeader() = default;
	H265FuHeader(const uint8_t *header) {parse(header);}

	void setPosition(Position pos) {_pos = pos;}
	Position getPosition() const {return _pos;}

	void setType(H265NaluType type) {_type = type;}
	H265NaluType getType() const {return _type;}

	void parse(const uint8_t *header);
	mblk_t *forge() const;

	static const size_t length = 1;

private:
	Position _pos = Position::Start;
	H265NaluType _type;
};

class H265ParameterSetsInserter: public H26xParameterSetsInserter {
public:
	~H265ParameterSetsInserter() {flush();}
	void process(MSQueue *in, MSQueue *out) override;
	void flush() override;

private:
	mblk_t *_vps = nullptr;
	mblk_t *_sps = nullptr;
	mblk_t *_pps = nullptr;
};

class H265ParameterSetsStore: public H26xParameterSetsStore {
public:
	H265ParameterSetsStore(): H26xParameterSetsStore({H265NaluType::Vps, H265NaluType::Sps, H265NaluType::Pps}) {}
	void addPs(mblk_t *nalu) override;
};

}
