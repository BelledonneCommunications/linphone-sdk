/*
 * Copyright (c) 2010-2025 Belledonne Communications SARL.
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

#ifndef _MS_GOERTZEL_STATE_H
#define _MS_GOERTZEL_STATE_H

namespace mediastreamer {

class GoertzelState {
public:
	GoertzelState() = default;
	~GoertzelState() = default;

	void init(int frequency, int samplingFrequency);

	float run(int16_t *samples, int nbSamples, float totalEnergy);

	uint64_t get_start_time() const {
		return mStartTime;
	}

	void set_start_time(uint64_t startTime) {
		mStartTime = startTime;
	}

	int get_duration() const {
		return mDuration;
	}

	void set_duration(int duration) {
		mDuration = duration;
	}

	bool is_event_sent() const {
		return mEventSent;
	}

	void set_event_sent(bool sent) {
		mEventSent = sent;
	}

private:
	uint64_t mStartTime = 0;
	int mDuration = 0;
	float mCoef = 0;
	bool mEventSent = false;
};

} // namespace mediastreamer

#endif /* _MS_GOERTZEL_STATE_H */
