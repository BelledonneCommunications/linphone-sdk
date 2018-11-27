package org.linphone.tester;

/*
 Tester.java
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

import android.content.Context;
import android.support.test.InstrumentationRegistry;

import org.junit.After;
import org.linphone.core.Factory;

import java.io.IOException;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;

public class Tester {
    public native void setApplicationContext(Context ct);
    public native void removeApplicationContext();
    public native int run(String args[]);
    public native void keepAccounts(boolean keep);
    public native void clearAccounts();

    public static String TAG = "LibLinphoneTester";

    private Context mContext;
    private boolean mHasBeenSetUp = false;

    private static Tester instance;

    public static Tester getInstance() {
        if (instance == null) {
            instance = new Tester();
        }
        return instance;
    }

    protected Tester() {

    }

    public void installTester() {
        if (!mHasBeenSetUp) {
            try {
                setUp();
                mHasBeenSetUp = true;
            } catch (IOException ioe) {

            }
        }
    }

    public boolean isReady() {
        return mHasBeenSetUp;
    }

    public int runTestInSuite(String suite, String test) {
        installTester();

        List<String> list = new LinkedList<>(Arrays.asList(new String[] {
                "tester",
                "--verbose",
                "--resource-dir", mContext.getFilesDir().getAbsolutePath(),
                "--writable-dir", mContext.getCacheDir().getPath(),
                "--suite", suite,
                "--test", test
        }));
        String[] array = list.toArray(new String[list.size()]);
        return run(array);
    }

    private void setUp() throws IOException {
        Factory.instance().setDebugMode(true, "LibLinphoneTester");
        System.loadLibrary("bctoolbox-tester");
        System.loadLibrary("linphonetester");

        keepAccounts(true);
        mContext = InstrumentationRegistry.getTargetContext();
        setApplicationContext(mContext);

        org.linphone.core.tools.AndroidPlatformHelper.copyAssetsFromPackage(mContext,"config_files", ".");
    }

    @After
    public void tearDown() {
        clearAccounts();
    }

    public void printLog(final int level, final String message) {
        switch(level) {
            case 0:
                android.util.Log.i(TAG, message);
                break;
            case 1:
                android.util.Log.e(TAG, message);
                break;
        }
    }
}
