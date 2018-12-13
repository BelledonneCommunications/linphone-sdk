package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class ProxyConfigTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("Proxy config");

        LinphoneTestSuite testsList = new LinphoneTestSuite();
        testsList.run(new String[]{"tester", "--list-tests", "Proxy config"});
        for (String testName: testsList.getList()) {
            LinphoneTest test = new LinphoneTest("Proxy config", testName);
            testSuites.addTest(test);
        }

        return testSuites;
    }
}

