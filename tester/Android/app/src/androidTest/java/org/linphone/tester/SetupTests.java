package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class SetupTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("Setup");

        LinphoneTestSuite testsList = new LinphoneTestSuite();
        testsList.run(new String[]{"tester", "--list-tests", "Setup"});
        for (String testName: testsList.getList()) {
            LinphoneTest test = new LinphoneTest("Setup", testName);
            testSuites.addTest(test);
        }

        return testSuites;
    }
}

