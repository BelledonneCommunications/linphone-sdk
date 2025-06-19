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

#include <exception>

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"

namespace mediastreamer {

class FilterBase {
public:
	class MethodCallFailed : public std::exception {};

	FilterBase(MSFilter *f) : _f(f) {
	}
	virtual ~FilterBase() = default;

	virtual void preprocess() = 0;
	virtual void process() = 0;
	virtual void postprocess() = 0;

protected:
	MSFactory *getFactory() const {
		return _f->factory;
	}
	MSQueue *getInput(int idx) const {
		return _f->inputs[idx];
	}
	MSQueue *getOutput(int idx) const {
		return _f->outputs[idx];
	}
	size_t getConnectedInputsCount() const {
		return static_cast<size_t>(_f->n_connected_inputs);
	}
	size_t getConnectedOutputsCount() const {
		return static_cast<size_t>(_f->n_connected_outputs);
	}
	uint64_t getTime() const {
		return _f->ticker->time;
	}
	MSTicker *getTicker() const {
		return _f->ticker;
	}

	void notify(unsigned int id) {
		ms_filter_notify_no_arg(_f, id);
	}

	void notify(unsigned int id, void *arg) {
		ms_filter_notify(_f, id, arg);
	}

	void lock() const {
		ms_filter_lock(_f);
	}
	void unlock() const {
		ms_filter_unlock(_f);
	}

private:
	MSFilter *_f = nullptr;
};

} // namespace mediastreamer
