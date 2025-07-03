/*
 * Copyright (c) 2010-2025 Belledonne Communications SARL.
 *
 * This file is part of Liblinphone 
 * (see https://gitlab.linphone.org/BC/public/liblinphone).
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
package org.linphone.core.tools.receiver;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.media.AudioManager;

import org.linphone.core.tools.Log;
import org.linphone.core.tools.AndroidPlatformHelper;

public class HdmiAudioReceiver extends BroadcastReceiver {
    public HdmiAudioReceiver() {
        super();
        Log.i("[HDMI] HDMI audio receiver created");
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent.getAction();
        Log.i("[HDMI] HDMI audio broadcast received");

        // https://developer.android.com/reference/android/media/AudioManager#ACTION_HDMI_AUDIO_PLUG
        if (action.equals(AudioManager.ACTION_HDMI_AUDIO_PLUG)) {
            int state = intent.getIntExtra(AudioManager.EXTRA_AUDIO_PLUG_STATE, 0);

            if (state == 0) {
                Log.i("[HDMI] HDMI audio disconnected");
            } else if (state == 1) {
                Log.i("[HDMI] HDMI audio connected");
            } else {
                Log.w("[HDMI] Unknown HDMI audio plugged state: " + state);
            }

            if (AndroidPlatformHelper.isReady()) AndroidPlatformHelper.instance().onHdmiStateChanged();
        }
    }
}