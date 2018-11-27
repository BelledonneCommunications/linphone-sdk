package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class ClonableObjectTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("ClonableObject");

        LinphoneTestSuite testsList = new LinphoneTestSuite();
        testsList.run(new String[]{"tester", "--list-tests", "ClonableObject"});
        for (String testName: testsList.getList()) {
            LinphoneTest test = new LinphoneTest("ClonableObject", testName);
            testSuites.addTest(test);
        }

        return testSuites;
    }
}

