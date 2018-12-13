package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class PropertyContainerTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("PropertyContainer");

        LinphoneTestSuite testsList = new LinphoneTestSuite();
        testsList.run(new String[]{"tester", "--list-tests", "PropertyContainer"});
        for (String testName: testsList.getList()) {
            LinphoneTest test = new LinphoneTest("PropertyContainer", testName);
            testSuites.addTest(test);
        }

        return testSuites;
    }
}

