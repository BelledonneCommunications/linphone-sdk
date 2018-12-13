package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class SingleCallTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("Single Call");

        LinphoneTestSuite testsList = new LinphoneTestSuite();
        testsList.run(new String[]{"tester", "--list-tests", "Single Call"});
        for (String testName: testsList.getList()) {
            LinphoneTest test = new LinphoneTest("Single Call", testName);
            testSuites.addTest(test);
        }

        return testSuites;
    }
}

