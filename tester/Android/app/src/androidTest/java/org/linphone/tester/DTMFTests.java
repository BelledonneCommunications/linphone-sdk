package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class DTMFTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("DTMF");

        LinphoneTestSuite testsList = new LinphoneTestSuite();
        testsList.run(new String[]{"tester", "--list-tests", "DTMF"});
        for (String testName: testsList.getList()) {
            LinphoneTest test = new LinphoneTest("DTMF", testName);
            testSuites.addTest(test);
        }

        return testSuites;
    }
}

