package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class PresenceUsingServerTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("Presence using server");

        LinphoneTestSuite testsList = new LinphoneTestSuite();
        testsList.run(new String[]{"tester", "--list-tests", "Presence using server"});
        for (String testName: testsList.getList()) {
            LinphoneTest test = new LinphoneTest("Presence using server", testName);
            testSuites.addTest(test);
        }

        return testSuites;
    }
}

