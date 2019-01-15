package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class VideoTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("Video");

        LinphoneTestSuite testsList = new LinphoneTestSuite();
        testsList.run(new String[]{"tester", "--list-tests", "Video"});
        for (String testName: testsList.getList()) {
            LinphoneTest test = new LinphoneTest("Video", testName);
            testSuites.addTest(test);
        }

        return testSuites;
    }
}

