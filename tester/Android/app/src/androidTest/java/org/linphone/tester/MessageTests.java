package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class MessageTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("Message");

        LinphoneTestSuite testsList = new LinphoneTestSuite();
        testsList.run(new String[]{"tester", "--list-tests", "Message"});
        for (String testName: testsList.getList()) {
            LinphoneTest test = new LinphoneTest("Message", testName);
            testSuites.addTest(test);
        }

        return testSuites;
    }
}

