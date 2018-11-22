package org.linphone.tester;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.support.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.linphone.core.Factory;
import org.linphone.core.LogCollectionState;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;

@RunWith(AndroidJUnit4.class)
@LargeTest
public class Tester {
    public native void setApplicationContext(Context ct);
    public native void removeApplicationContext();
    public native int run(String args[]);
    public native void keepAccounts(boolean keep);
    public native void clearAccounts();

    public static String TAG = "LibLinphoneTester";

    private Context mContext;

    @Before
    public void setUp() throws IOException {
        Factory.instance().enableLogCollection(LogCollectionState.Enabled);
        Factory.instance().setDebugMode(true, "LibLinphoneTester");
        System.loadLibrary("bctoolbox-tester");
        System.loadLibrary("linphonetester");

        printLog(0, "Setting up...");
        keepAccounts(true);
        mContext = InstrumentationRegistry.getTargetContext();
        setApplicationContext(mContext);

        org.linphone.core.tools.AndroidPlatformHelper.copyAssetsFromPackage(mContext,"config_files", ".");
    }

    @Test
    public void runTests() {
        List<String> list = new LinkedList<>(Arrays.asList(new String[] {
                "tester",
                "--verbose",
                "--resource-dir", mContext.getFilesDir().getAbsolutePath(),
                "--writable-dir", mContext.getCacheDir().getPath()
        }));
        String[] array = list.toArray(new String[list.size()]);
        run(array);
    }

    @After
    public void tearDown() {
        printLog(0, "Tearing down...");
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
