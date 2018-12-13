package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class CpimTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("Cpim");

        LinphoneTestSuite testsList = new LinphoneTestSuite();
        testsList.run(new String[]{"tester", "--list-tests", "Cpim"});
        for (String testName: testsList.getList()) {
            LinphoneTest test = new LinphoneTest("Cpim", testName);
            testSuites.addTest(test);
        }

        return testSuites;
    }
}

