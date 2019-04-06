package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class SecureGroupChatTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("Secure Group Chat");

        LinphoneTestSuite testsList = new LinphoneTestSuite();
        testsList.run(new String[]{"tester", "--list-tests", "Secure Group Chat"});
        for (String testName: testsList.getList()) {
            LinphoneTest test = new LinphoneTest("Secure Group Chat", testName);
            testSuites.addTest(test);
        }

        return testSuites;
    }
}

