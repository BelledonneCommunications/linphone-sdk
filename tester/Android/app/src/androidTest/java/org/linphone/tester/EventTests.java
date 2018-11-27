package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class EventTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("Event");

        LinphoneTestSuite testsList = new LinphoneTestSuite();
        testsList.run(new String[]{"tester", "--list-tests", "Event"});
        for (String testName: testsList.getList()) {
            LinphoneTest test = new LinphoneTest("Event", testName);
            testSuites.addTest(test);
        }

        return testSuites;
    }
}

