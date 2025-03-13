[Build SDK and activate Swift tests (execute command lines from linphone-sdk dir)]
cmake --preset=ios-sdk -G Ninja -B IOS_TESTER -DLINPHONESDK_IOS_ARCHS=arm64 -DENABLE_UNIT_TESTS=YES
cmake --build IOS_TESTER --config RelWithDebInfo -j8 

Test execution: 
cd tester/IOS/swift/LinphoneTester

[Tests execution]
- Replace “IPHONE_DESTINATION“ with the device the tests will be run on. 
- Available devices can be displayed by running “xcrun xctrace list devices”
- “test” can be replaced with “test-without-building”
- “TEST_OUTPUT_DIR” must not already exists. It will be generated, as well as “TEST_OUTPUT_DIR.xcresult”

 xcodebuild -scheme LinphoneTester -destination 'platform=iOS,name=IPHONE_DESTINATION' test -testPlan SwiftTests -resultBundlePath TEST_OUTPUT_DIR

[Process xcresult]

brew install chargepoint/xcparse/xcparse
sudo gem install trainer

- Running “trainer” in the directory where you wrote TEST_OUTPUT_DIR will automatically generate “TEST_OUTPUT.xml”
- Running “xcparse attachments TEST_OUTPUT.xcresult LOGS_DIR --uti public.plain-text” will extract the logs from the failed tests in “LOGS_DIR”
