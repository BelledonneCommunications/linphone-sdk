package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class MainDbTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("MainDb");

        LinphoneTestSuite testsList = new LinphoneTestSuite();
        testsList.run(new String[]{"tester", "--list-tests", "MainDb"});
        for (String testName: testsList.getList()) {
            LinphoneTest test = new LinphoneTest("MainDb", testName);
            testSuites.addTest(test);
        }

        return testSuites;
    }
}

