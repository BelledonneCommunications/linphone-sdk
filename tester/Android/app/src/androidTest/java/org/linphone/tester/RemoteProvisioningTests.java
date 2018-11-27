package org.linphone.tester;

import junit.framework.TestSuite;

import org.junit.runner.RunWith;

@RunWith(org.junit.runners.AllTests.class)
public class RemoteProvisioningTests {
    public static TestSuite suite() {
        TestSuite testSuites = new TestSuite();
        testSuites.setName("RemoteProvisioning");

        LinphoneTestSuite testsList = new LinphoneTestSuite();
        testsList.run(new String[]{"tester", "--list-tests", "RemoteProvisioning"});
        for (String testName: testsList.getList()) {
            LinphoneTest test = new LinphoneTest("RemoteProvisioning", testName);
            testSuites.addTest(test);
        }

        return testSuites;
    }
}

