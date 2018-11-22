package org.linphone.tester;

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
