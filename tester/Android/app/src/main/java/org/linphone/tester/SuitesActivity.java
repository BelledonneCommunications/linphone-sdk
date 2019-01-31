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
import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import androidx.core.app.ActivityCompat;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;

import org.linphone.mediastream.Version;

import java.util.ArrayList;

public class SuitesActivity extends Activity implements View.OnClickListener {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_suites);
        LinearLayout suitesLayout = findViewById(R.id.suitesLayout);

        Tester.getInstance().setContext(getApplicationContext());

        Button button = new Button(this);
        button.setText("All");
        button.setTag("All");
        suitesLayout.addView(button);
        button.setOnClickListener(this);

        LinphoneTestSuite suitesList = new LinphoneTestSuite();
        suitesList.run(new String[]{"tester", "--list-suites"});
        for (String suite : suitesList.getList()) {
            button = new Button(this);
            button.setText(suite);
            button.setTag(suite);
            suitesLayout.addView(button);
            button.setOnClickListener(this);
        }
    }

    @Override
    public void onClick(View v) {
        Intent intent = new Intent(this, TestsActivity.class);
        intent.putExtra("Suite", (String)v.getTag());
        startActivity(intent);
    }

    @Override
    protected void onStart() {
        super.onStart();
        if (Version.sdkAboveOrEqual(Version.API23_MARSHMALLOW_60)) {
            askForPermissions();
        }
    }

    @TargetApi(23)
    private void askForPermissions() {
        ArrayList<String> permissionsList = new ArrayList<>();
        permissionsList.add(Manifest.permission.RECORD_AUDIO);
        permissionsList.add(Manifest.permission.CAMERA);
        permissionsList.add(Manifest.permission.READ_PHONE_STATE);
        permissionsList.add(Manifest.permission.WRITE_EXTERNAL_STORAGE);
        if (permissionsList.size() > 0) {
            String[] permissions = new String[permissionsList.size()];
            permissions = permissionsList.toArray(permissions);
            requestPermissions(permissions, 0);
        }
    }
}
