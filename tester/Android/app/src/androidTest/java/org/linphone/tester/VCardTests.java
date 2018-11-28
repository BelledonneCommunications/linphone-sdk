package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class VCardTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("VCard");

        LinphoneTestSuite testsList = new LinphoneTestSuite();
        testsList.run(new String[]{"tester", "--list-tests", "VCard"});
        for (String testName: testsList.getList()) {
            LinphoneTest test = new LinphoneTest("VCard", testName);
            testSuites.addTest(test);
        }

        return testSuites;
    }
}

