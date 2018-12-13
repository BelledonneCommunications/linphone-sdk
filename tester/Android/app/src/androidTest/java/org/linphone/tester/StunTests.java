package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class StunTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("Stun");

        LinphoneTestSuite testsList = new LinphoneTestSuite();
        testsList.run(new String[]{"tester", "--list-tests", "Stun"});
        for (String testName: testsList.getList()) {
            LinphoneTest test = new LinphoneTest("Stun", testName);
            testSuites.addTest(test);
        }

        return testSuites;
    }
}

