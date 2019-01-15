package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class UtilsTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("Utils");

        LinphoneTestSuite testsList = new LinphoneTestSuite();
        testsList.run(new String[]{"tester", "--list-tests", "Utils"});
        for (String testName: testsList.getList()) {
            LinphoneTest test = new LinphoneTest("Utils", testName);
            testSuites.addTest(test);
        }

        return testSuites;
    }
}

