package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class PresenceTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("Presence");

        LinphoneTestSuite testsList = new LinphoneTestSuite();
        testsList.run(new String[]{"tester", "--list-tests", "Presence"});
        for (String testName: testsList.getList()) {
            LinphoneTest test = new LinphoneTest("Presence", testName);
            testSuites.addTest(test);
        }

        return testSuites;
    }
}

