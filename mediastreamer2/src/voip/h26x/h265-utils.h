/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
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

#include "h26x-utils.h"

namespace mediastreamer {

class H265NaluType : public H26xNaluType {
public:
	H265NaluType() = default;
	H265NaluType(uint8_t value);

	bool isVcl() const override {
		return _value < 32;
	}
	bool isParameterSet() const override {
		return *this == Vps || *this == Sps || *this == Pps;
	}
	bool isKeyFramePart() const override {
		return *this == IdrNLp || *this == IdrWRadl || *this == CraNut;
	}

	static const H265NaluType IdrWRadl;
	static const H265NaluType IdrNLp;
	static const H265NaluType CraNut;
	static const H265NaluType Vps;
	static const H265NaluType Sps;
	static const H265NaluType Pps;
	static const H265NaluType Ap;
	static const H265NaluType Fu;
};

class H265NaluHeader : public H26xNaluHeader {
public:
	H265NaluHeader() : H26xNaluHeader(){};
	H265NaluHeader(const uint8_t *header) {
		parse(header);
	}

	void setType(H265NaluType type) {
		_type = type;
	}
	H265NaluType getType() const {
		return _type;
	}

	const H26xNaluType &getAbsType() const override {
		return _type;
	}

	void setLayerId(uint8_t layerId);
	uint8_t getLayerId() const {
		return _layerId;
	}

	void setTid(uint8_t tid);
	uint8_t getTid() const {
		return _tid;
	}

	bool operator==(const H265NaluHeader &h2) const;
	bool operator!=(const H265NaluHeader &h2) const {
		return !(*this == h2);
	}

	void parse(const uint8_t *header) override;
	mblk_t *forge() const override;

	static const size_t length = 2;

private:
	H265NaluType _type;
	uint8_t _layerId = 0;
	uint8_t _tid = 0;
};

class H265FuHeader {
public:
	enum class Position { Start, Middle, End };

	H265FuHeader() = default;
	H265FuHeader(const uint8_t *header) {
		parse(header);
	}

	void setPosition(Position pos) {
		_pos = pos;
	}
	Position getPosition() const {
		return _pos;
	}

	void setType(H265NaluType type) {
		_type = type;
	}
	H265NaluType getType() const {
		return _type;
	}

	void parse(const uint8_t *header);
	mblk_t *forge() const;

	static const size_t length = 1;

private:
	Position _pos = Position::Start;
	H265NaluType _type;
};

class H265ParameterSetsInserter : public H26xParameterSetsInserter {
public:
	~H265ParameterSetsInserter() {
		flush();
	}
	void process(MSQueue *in, MSQueue *out) override;
	void flush() override;

private:
	mblk_t *_vps = nullptr;
	mblk_t *_sps = nullptr;
	mblk_t *_pps = nullptr;
};

class H265ParameterSetsStore : public H26xParameterSetsStore {
public:
	H265ParameterSetsStore()
	    : H26xParameterSetsStore("video/hevc", {H265NaluType::Vps, H265NaluType::Sps, H265NaluType::Pps}) {
	}
};

class H265ToolFactory : public H26xToolFactory {
public:
	H26xNaluHeader *createNaluHeader() const override;
	NalPacker *createNalPacker(size_t maxPayloadType) const override;
	NalUnpacker *createNalUnpacker() const override;
	H26xParameterSetsInserter *createParameterSetsInserter() const override;
	H26xParameterSetsStore *createParameterSetsStore() const override;
};

} // namespace mediastreamer
