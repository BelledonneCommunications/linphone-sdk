package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class TunnelTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("Tunnel");

        LinphoneTestSuite testsList = new LinphoneTestSuite();
        testsList.run(new String[]{"tester", "--list-tests", "Tunnel"});
        for (String testName: testsList.getList()) {
            LinphoneTest test = new LinphoneTest("Tunnel", testName);
            testSuites.addTest(test);
        }

        return testSuites;
    }
}

