package org.linphone.tester;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;

public class TestsActivity extends Activity implements View.OnClickListener {
    private String mSuiteName;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_tests);
        LinearLayout testsLayout = findViewById(R.id.testsLayout);

        Tester.getInstance().setContext(getApplicationContext());

        String suiteName = getIntent().getStringExtra("Suite");
        if (suiteName.equals("All")) {
            Intent intent = new Intent(this, LogsActivity.class);
            intent.putExtra("Suite", "All");
            intent.putExtra("Test", "All");
            startActivity(intent);
            return;
        }
        mSuiteName = suiteName;

        Button button = new Button(this);
        button.setText("All");
        button.setTag("All");
        testsLayout.addView(button);
        button.setOnClickListener(this);

        LinphoneTestSuite suitesList = new LinphoneTestSuite();
        suitesList.run(new String[]{"tester", "--list-tests", suiteName});
        for (String test : suitesList.getList()) {
            button = new Button(this);
            button.setText(test);
            button.setTag(test);
            testsLayout.addView(button);
            button.setOnClickListener(this);
        }
    }

    @Override
    public void onClick(View v) {
        Intent intent = new Intent(this, LogsActivity.class);
        intent.putExtra("Suite", mSuiteName);
        intent.putExtra("Test", (String)v.getTag());
        startActivity(intent);
    }
}
