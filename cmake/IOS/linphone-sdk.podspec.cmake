Pod::Spec.new do |s|

  s.name         = "linphone-sdk"
  s.version      = "@LINPHONESDK_VERSION@"
  s.summary      = "Liblinphone is a high-level SIP library integrating all calling and instant messaging features into a single easy-to-use API. It is the cross-platform VoIP library on which Linphone is based, and that anyone can use to add audio and video calls or instant messaging capabilities to an application."
  s.description  = <<-PODSPEC_DESC
@LINPHONESDK_ENABLED_FEATURES@PODSPEC_DESC
  s.homepage     = "https://github.com/BelledonneCommunications/linphone-iphone"
  s.license      = { :type => "GNU GPL 3", :text => <<-LICENSE
@LINPHONESDK_LICENSE@LICENSE
    }
  s.author       = { 'Belledonne Communications SARL' => 'linphone-iphone@belledonne-communications.com' }
  s.platform     = :ios, "9.0"
  s.source       = { :http => "@LINPHONESDK_IOS_BASE_URL@/linphone-sdk-ios-@LINPHONESDK_VERSION@.zip" }
  s.vendored_frameworks = "linphone-sdk/apple-darwin/Frameworks/**"
  s.pod_target_xcconfig = { 'VALID_ARCHS' => "@VALID_ARCHS@" }
  s.resource = "linphone-sdk/apple-darwin/Resources/**"
  s.subspec '@LINPHONE_SUBSPEC_NAME@' do |basic|
    basic.vendored_frameworks = "linphone-sdk/apple-darwin/Frameworks/{@LINPHONE_SUBSPEC_FRAMEWORKS@}"
  end

end
