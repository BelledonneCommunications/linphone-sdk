Pod::Spec.new do |s|

  s.name         = "linphone-sdk"
  s.version      = "@LINPHONESDK_VERSION@"
  s.summary      = "This is the linphone SDK, a free (GPL) video voip library based on the SIP protocol"
  s.description  = <<-PODSPEC_DESC
@LINPHONESDK_ENABLED_FEATURES@PODSPEC_DESC
  s.homepage     = "https://github.com/BelledonneCommunications/linphone-desktop"
  s.license      = { :type => "GNU GPL 2", :text => <<-LICENSE
@LINPHONESDK_LICENSE@LICENSE
    }
  s.author       = "employees@belledonne-communications.com"
  s.platform     = :osx, "10.9"
  s.source       = { :http => "@LINPHONESDK_MACOS_BASE_URL@/linphone-sdk-@LINPHONESDK_VERSION@.zip" }
  s.vendored_frameworks = "linphone-sdk/desktop/Frameworks/**"

end
