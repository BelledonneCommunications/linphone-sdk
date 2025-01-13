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

#include <cmath>
#include <cstdint>

#include "goertzel_state.h"

static const double PI = std::acos(-1);

namespace mediastreamer {

void GoertzelState::init(int frequency, int samplingFrequency) {
	mCoef = (float)2.0f * (float)std::cos(2 * PI * ((float)frequency / (float)samplingFrequency));
	mStartTime = 0;
	mDuration = 0;
}

float GoertzelState::run(int16_t *samples, int nbSamples, float totalEnergy) {
	float tmp;
	float q1 = 0;
	float q2 = 0;
	float frequencyEnergy;

	for (int i = 0; i < nbSamples; ++i) {
		tmp = q1;
		q1 = (mCoef * q1) - q2 + (float)samples[i];
		q2 = tmp;
	}

	frequencyEnergy = (q1 * q1) + (q2 * q2) - (q1 * q2 * mCoef);

	/* Return a relative frequency energy compared over the total signal energy */
	return frequencyEnergy / (totalEnergy * (float)nbSamples * 0.5f);
}

} // namespace mediastreamer