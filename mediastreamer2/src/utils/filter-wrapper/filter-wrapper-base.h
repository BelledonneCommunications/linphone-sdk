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

#include "mediastreamer2/msfilter.h"

#include "filter-interface/filter-base.h"

namespace mediastreamer {

class FilterWrapperBase {
public:
	template <class T>
	static void onFilterInit(MSFilter *f) {
		f->data = new T(f);
	}

	static void onFilterUninit(MSFilter *f) {
		delete static_cast<FilterBase *>(f->data);
	}

	static void onFilterPreProcess(MSFilter *f) {
		static_cast<FilterBase *>(f->data)->preprocess();
	}

	static void onFilterPostProcess(MSFilter *f) {
		static_cast<FilterBase *>(f->data)->postprocess();
	}

	static void onFilterProcces(MSFilter *f) {
		static_cast<FilterBase *>(f->data)->process();
	}
};

}; // namespace mediastreamer

#define MS_FILTER_WRAPPER_NAME(base_name) FilterWrapperBase
#define MS_FILTER_WRAPPER_METHODS_NAME(base_name) ms_##base_name##_methods

#define MS_FILTER_WRAPPER_FILTER_DESCRIPTION_BASE(base_name, id, text, category, enc_fmt, ninputs, noutputs, flags)    \
	extern "C" MSFilterDesc ms_##base_name##_desc = {id,                                                               \
	                                                 "MS" #base_name,                                                  \
	                                                 text,                                                             \
	                                                 category,                                                         \
	                                                 enc_fmt,                                                          \
	                                                 ninputs,                                                          \
	                                                 noutputs,                                                         \
	                                                 FilterWrapperBase::onFilterInit<base_name##FilterImpl>,           \
	                                                 FilterWrapperBase::onFilterPreProcess,                            \
	                                                 FilterWrapperBase::onFilterProcces,                               \
	                                                 FilterWrapperBase::onFilterPostProcess,                           \
	                                                 FilterWrapperBase::onFilterUninit,                                \
	                                                 MS_FILTER_WRAPPER_METHODS_NAME(base_name),                        \
	                                                 flags}
