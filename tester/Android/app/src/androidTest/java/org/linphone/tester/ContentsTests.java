package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class ContentsTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("Contents");

        LinphoneTestSuite testsList = new LinphoneTestSuite();
        testsList.run(new String[]{"tester", "--list-tests", "Contents"});
        for (String testName: testsList.getList()) {
            LinphoneTest test = new LinphoneTest("Contents", testName);
            testSuites.addTest(test);
        }

        return testSuites;
    }
}

