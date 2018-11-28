package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class PlayerTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("Player");

        LinphoneTestSuite testsList = new LinphoneTestSuite();
        testsList.run(new String[]{"tester", "--list-tests", "Player"});
        for (String testName: testsList.getList()) {
            LinphoneTest test = new LinphoneTest("Player", testName);
            testSuites.addTest(test);
        }

        return testSuites;
    }
}

