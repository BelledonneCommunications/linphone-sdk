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

import android.Manifest;
import android.app.Activity;
import android.os.Bundle;
import java.util.ArrayList;

public class SuitesActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_suites);
    }

    @Override
    protected void onStart() {
        super.onStart();

        ArrayList<String> permissionsList = new ArrayList<>();
        permissionsList.add(Manifest.permission.READ_EXTERNAL_STORAGE);
        permissionsList.add(Manifest.permission.WRITE_EXTERNAL_STORAGE);

        String[] permissions = new String[permissionsList.size()];
        permissions = permissionsList.toArray(permissions);
        requestPermissions(permissions, 0);
    }
}
