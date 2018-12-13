package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class AudioBypassTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("Audio Bypass");

        LinphoneTestSuite testsList = new LinphoneTestSuite();
        testsList.run(new String[]{"tester", "--list-tests", "Audio Bypass"});
        for (String testName: testsList.getList()) {
            LinphoneTest test = new LinphoneTest("Audio Bypass", testName);
            testSuites.addTest(test);
        }

        return testSuites;
    }
}

