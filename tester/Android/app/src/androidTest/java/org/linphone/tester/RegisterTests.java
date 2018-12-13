package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class RegisterTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("Register");

        LinphoneTestSuite testsList = new LinphoneTestSuite();
        testsList.run(new String[]{"tester", "--list-tests", "Register"});
        for (String testName: testsList.getList()) {
            LinphoneTest test = new LinphoneTest("Register", testName);
            testSuites.addTest(test);
        }

        return testSuites;
    }
}

