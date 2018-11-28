package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class FlexisipTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("Flexisip");

        LinphoneTestSuite testsList = new LinphoneTestSuite();
        testsList.run(new String[]{"tester", "--list-tests", "Flexisip"});
        for (String testName: testsList.getList()) {
            LinphoneTest test = new LinphoneTest("Flexisip", testName);
            testSuites.addTest(test);
        }

        return testSuites;
    }
}

