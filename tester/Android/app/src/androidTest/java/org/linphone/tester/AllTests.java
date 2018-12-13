package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class AllTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("All suites");

        LinphoneTestSuite suitesList = new LinphoneTestSuite();
        suitesList.run(new String[]{"tester", "--list-suites"});
        for (String suiteName : suitesList.getList()) {
            LinphoneTestSuite testsList = new LinphoneTestSuite();
            testsList.run(new String[]{"tester", "--list-tests", suiteName});
            for (String testName: testsList.getList()) {
                LinphoneTest test = new LinphoneTest(suiteName, testName);
                testSuites.addTest(test);
            }
        }
        return testSuites;
    }
}
