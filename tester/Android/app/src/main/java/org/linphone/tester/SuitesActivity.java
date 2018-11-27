package org.linphone.tester;

/*
 SuitesActivity.java
 Copyright (C) 2018  Belledonne Communications, Grenoble, France

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

import android.app.Activity;
import android.os.Bundle;

public class SuitesActivity extends Activity {
    private static String[] sSuites = {"Setup", "Register", "Tunnel", "Offer-answer", "Single Call",
            "Video Call", "Audio Bypass", "Multi call", "Message", "Presence",
            "Presence using server", "Account creator", "Stun", "Event", "Conference event",
            "Contents", "Flexisip", "RemoteProvisioning", "QualityReporting", "LogCollection",
            "Player", "DTMF", "Cpim", "Multipart", "ClonableObject", "MainDb", "PropertyContainer",
            "Video", "Multicast Call", "Proxy config", "VCard", "Group Chat", "Utils", "Setup",
            "Register", "Tunnel", "Offer-answer", "Single Call", "Video Call", "Audio Bypass",
            "Multi call", "Message", "Presence", "Presence using server", "Account creator", "Stun",
            "Event", "Conference event", "Contents", "Flexisip", "RemoteProvisioning",
            "QualityReporting", "LogCollection", "Player", "DTMF", "Cpim", "Multipart",
            "ClonableObject", "MainDb", "PropertyContainer", "Video", "Multicast Call",
            "Proxy config", "VCard", "Group Chat", "Utils"};

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_suites);
    }
}
