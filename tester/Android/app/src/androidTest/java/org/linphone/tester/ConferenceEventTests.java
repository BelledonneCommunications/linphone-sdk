package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class ConferenceEventTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("Conference event");

        LinphoneTestSuite testsList = new LinphoneTestSuite();
        testsList.run(new String[]{"tester", "--list-tests", "Conference event"});
        for (String testName: testsList.getList()) {
            LinphoneTest test = new LinphoneTest("Conference event", testName);
            testSuites.addTest(test);
        }

        return testSuites;
    }
}

