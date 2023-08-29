/*
 * Copyright (c) 2016-2023 Belledonne Communications SARL.
 *
 * This file is part of bctoolbox.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bctoolbox/defs.h"
#include "bctoolbox/logging.h"

#include <list>
#include <map>
#include <stack>
#include <string>

using namespace std;

namespace bctoolbox {

class LogTags {
public:
	static LogTags &get() {
		return sThreadLocalInstance;
	}
	void pushTag(const string &tagType, const string &tagValue) {
		auto &tagStack = mTags[tagType];
		if (tagStack.empty() || tagStack.top().mValue != tagValue) {
			tagStack.push({tagValue, 1});
		} else {
			tagStack.top().mCount++;
		}
		mTagsModfied = true;
	}
	void popTag(const string &tagType) {
		auto &tagStack = mTags[tagType];
		if (tagStack.empty()) {
			bctbx_error("logging: no tag type '%s' pushed previously. Check your code.", tagType.c_str());
			return;
		} else {
			if (--tagStack.top().mCount == 0) {
				tagStack.pop();
				mTagsModfied = true;
			}
		}
	}
	const list<string> &getTags() {
		if (mTagsModfied) {
			mCurrentTags.clear();
			bctbx_list_free(mCurrentTagsCList);
			mCurrentTagsCList = nullptr;
			for (auto it = mTags.rbegin(); it != mTags.rend(); ++it) {
				auto &tagStack = (*it).second;
				if (!tagStack.empty()) {
					const string &value = tagStack.top().mValue;
					mCurrentTags.emplace_front(value);
					mCurrentTagsCList = bctbx_list_prepend(mCurrentTagsCList, const_cast<char *>(value.c_str()));
				}
			}
			mTagsModfied = false;
		}
		return mCurrentTags;
	}
	const bctbx_list_t *getTagsAsCList() {
		getTags();
		return mCurrentTagsCList;
	}
	/* bctbx_log_tags_t is the C opaque typedef for the ContextCopy */
	typedef map<string, string> ContextCopy;
	ContextCopy createCopy() {
		ContextCopy ret;
		for (auto &tag : mTags) {
			auto &tagStack = tag.second;
			if (!tagStack.empty()) {
				ret[tag.first] = tagStack.top().mValue;
			}
		}
		return ret;
	}
	void paste(const ContextCopy &ctx) {
		if (!mTags.empty()) {
			bctbx_error("logging: assigning log tags to an non-empty context - not recommended, pre-existing tags will "
			            "be lost.");
		}
		for (auto &p : ctx) {
			pushTag(p.first, p.second);
		}
	}
	~LogTags() {
		bctbx_list_free(mCurrentTagsCList);
	}

private:
	LogTags() = default;
	struct TagValue {
		string mValue;
		int mCount = 0;
	};
	map<string, stack<TagValue>> mTags;
	list<string> mCurrentTags;
	bctbx_list_t *mCurrentTagsCList = nullptr;
	bool mTagsModfied = false;
	thread_local static LogTags sThreadLocalInstance;
};

thread_local LogTags LogTags::sThreadLocalInstance;

} // namespace bctoolbox

void bctbx_push_log_tag(const char *tag_identifier, const char *tag_value) {
	bctoolbox::LogTags::get().pushTag(tag_identifier, tag_value);
}

void bctbx_pop_log_tag(const char *tag_identifier) {
	bctoolbox::LogTags::get().popTag(tag_identifier);
}

const bctbx_list_t *bctbx_get_log_tags(void) {
	return bctoolbox::LogTags::get().getTagsAsCList();
}

bctbx_log_tags_t *bctbx_create_log_tags_copy(void) {
	return (bctbx_log_tags_t *)new bctoolbox::LogTags::ContextCopy(bctoolbox::LogTags::get().createCopy());
}

BCTBX_PUBLIC void bctbx_paste_log_tags(const bctbx_log_tags_t *log_tags) {
	bctoolbox::LogTags::get().paste(*(const bctoolbox::LogTags::ContextCopy *)log_tags);
}

BCTBX_PUBLIC void bctbx_log_tags_destroy(bctbx_log_tags_t *log_tags) {
	delete (bctoolbox::LogTags::ContextCopy *)log_tags;
}
