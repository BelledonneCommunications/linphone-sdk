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

import org.junit.After;
import org.junit.Before;
import org.linphone.core.Factory;

import java.io.IOException;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;

import androidx.test.platform.app.InstrumentationRegistry;

public class Tester {
    public native void setApplicationContext(Context ct);
    public native void removeApplicationContext();
    public native int run(String args[]);
    public native String run2(String args[]);
    public native void keepAccounts(boolean keep);
    public native void clearAccounts();
    public native String getFailedAsserts();

    public static String TAG = "LibLinphoneTester";

    private Context mContext;
    private boolean mHasBeenSetUp = false;
    private TesterLogListener mListener;

    private static Tester instance;

    public static Tester getInstance() {
        if (instance == null) {
            instance = new Tester();
        }
        return instance;
    }

    protected Tester() {

    }

    public void setContext(Context context) {
        mContext = context;
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

    public void setListener(TesterLogListener listener) {
        mListener = listener;
    }

    public String runTestInSuite(String suite, String test) {
        installTester();

        List<String> list = new LinkedList<>(Arrays.asList(new String[] {
                "tester",
                "--verbose",
                "--resource-dir", mContext.getFilesDir().getAbsolutePath(),
                "--writable-dir", mContext.getCacheDir().getPath()
        }));

        if (suite != null && suite.length() > 0) {
            list.add("--suite");
            list.add(suite);
            if (test != null && test.length() > 0) {
                list.add("--test");
                list.add(test);
            }
        }

        String[] array = list.toArray(new String[list.size()]);
        return run2(array);
    }

    private void setUp() throws IOException {
        Factory.instance().setDebugMode(true, "LibLinphoneTester");
        System.loadLibrary("bctoolbox-tester");
        System.loadLibrary("linphonetester");

        //keepAccounts(false);
        keepAccounts(true);

        if (mContext == null) {
            mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        }
        setApplicationContext(mContext);

        org.linphone.core.tools.AndroidPlatformHelper.copyAssetsFromPackage(mContext,"config_files", ".");
    }

    @Before
    public void initTest() {
        if (mContext == null) {
            mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        }
        setApplicationContext(mContext);
    }

    @After
    public void tearDown() {
        clearAccounts();
        removeApplicationContext();
    }

    public void printLog(final int level, final String message) {
        switch(level) {
            case 0:
                android.util.Log.i(TAG, message);
                if (mListener != null) {
                    mListener.onMessage(message);
                }
                break;
            case 1:
                android.util.Log.e(TAG, message);
                if (mListener != null) {
                    mListener.onError(message);
                }
                break;
        }
    }
}
