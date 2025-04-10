[Build SDK and activate Swift tests (execute command lines from linphone-sdk dir)]
cmake --preset=ios-sdk -G Ninja -B IOS_TESTER -DLINPHONESDK_IOS_ARCHS=arm64 -DENABLE_UNIT_TESTS=YES
cmake --build IOS_TESTER --config RelWithDebInfo -j8 

/!\ If you build in a folder that's not "linphone-sdk/IOS_TESTER", make sure to update the path to the linphone-sdk-swift-ios package in the xcproject /!\

Test execution: 
cd tester/IOS/swift/LinphoneTester

[Xcode tests execution]
- Go to testplan "SwiftTests" configuration
- In the environment variable, replace  "LINPHONETESTER_FLEXISIP_DNS_ENV_VAR" with the correct test server
- run tests

[Command line tests execution]
- Replace "$TESTER_ADDRESSS" with the correct test server
- Replace “$IPHONE_DESTINATION“ with the device the tests will be run on. 
- Available devices can be displayed by running “xcrun xctrace list devices”
- “test” can be replaced with “test-without-building”
- “TEST_OUTPUT_DIR” must not already exists. It will be generated, as well as “TEST_OUTPUT_DIR.xcresult”

 xcodebuild -scheme LinphoneTester -destination 'platform=iOS,name=$IPHONE_DESTINATION' test -resultBundlePath TEST_OUTPUT_DIR LINPHONETESTER_FLEXISIP_DNS=$TESTER_ADDRESSS

[Process xcresult]
brew install chargepoint/xcparse/xcparse
sudo gem install trainer

- Running “trainer” in the directory where you wrote TEST_OUTPUT_DIR will automatically generate “TEST_OUTPUT.xml”
- Running “xcparse attachments TEST_OUTPUT.xcresult LOGS_DIR --uti public.plain-text” will extract the logs from the failed tests in “LOGS_DIR”

[Adding new tests to the project]
To add new tests to the project, there are two cases: 
1) If you wish to add new push related tests, just add "func testMyTestName" to the PhysicalDeviceIncomingPushTests class

2) For other swift tests, create a new MyTestSuiteName : XCTestCase class. In that class, make sure to implement:
- override class func setUp() : this is called once, wether you run the whole test class or a single instance. 
- class func setUp() : called at the start of each test
- class func tearDown() : called at the end of each test, no matter the result

The LinphoneTesterEnvironment is a singleton, however static variables in swift are lazy-initialized. Make sure to call it so that it gets properly initialized.
