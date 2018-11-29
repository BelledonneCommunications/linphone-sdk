Pod::Spec.new do |s|

  s.name         = "linphone-sdk"
  s.version      = "@LINPHONESDK_VERSION@"
  s.summary      = "This is the linphone SDK, a free (GPL) video voip library based on the SIP protocol"
  s.description  = <<-PODSPEC_DESC
@LINPHONESDK_ENABLED_FEATURES@PODSPEC_DESC
  s.homepage     = "https://github.com/BelledonneCommunications/linphone-iphone"
  s.license      = { :type => "GNU GPL 2", :text => <<-LICENSE
@LINPHONESDK_LICENSE@LICENSE
    }
  s.author       = "employees@belledonne-communications.com" 
  s.platform     = :ios, "9.0"
  s.source       = { :http => "@LINPHONESDK_IOS_BASE_URL@/linphone-sdk-ios-@LINPHONESDK_VERSION@.zip" }
  s.vendored_frameworks = "linphone-sdk/apple-darwin/Frameworks/**"
  s.resource = "linphone-sdk/apple-darwin/Resources/**"
  s.pod_target_xcconfig = { 'VALID_ARCHS' => "@VALID_ARCHS@" }

end
