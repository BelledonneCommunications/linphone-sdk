package org.linphone.tester;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.text.Html;
import android.widget.TextView;

public class LogsActivity extends Activity implements TesterLogListener {
    private String mSuite, mTest;
    private TextView mLogs;
    private String mHtmlLogs;
    private Handler mHandler = new Handler();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_logs);
        mLogs = findViewById(R.id.logs);

        mSuite = getIntent().getStringExtra("Suite");
        mTest = getIntent().getStringExtra("Test");

        Tester.getInstance().setContext(getApplicationContext());
    }

    @Override
    protected void onStart() {
        super.onStart();

        Tester.getInstance().setListener(this);
        new Thread() {
            @Override
            public void run() {
                Tester.getInstance().runTestInSuite(mSuite.equals("All") ? null : mSuite, mTest.equals("All") ? null : mTest);
            }
        }.start();
    }

    @Override
    public void onMessage(final String message) {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                mHtmlLogs += "<p>" + message + "</p>";
                mLogs.setText(Html.fromHtml(mHtmlLogs));
            }
        });
    }

    @Override
    public void onError(final String message) {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                mHtmlLogs += "<p><font color=#ff0000>" + message + "</font></p>";
                mLogs.setText(Html.fromHtml(mHtmlLogs));
            }
        });
    }
}
