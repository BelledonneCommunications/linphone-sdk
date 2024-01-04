Pod::Spec.new do |s|

  s.name         = "@LINPHONESDK_NAME@"
  s.version      = "@LINPHONESDK_VERSION@"
  s.summary      = "Liblinphone is a library to create VoIP and IM apps, based on SIP protocol."
  s.description  = <<-PODSPEC_DESC
@LINPHONESDK_ENABLED_FEATURES@PODSPEC_DESC
  s.homepage     = "https://github.com/BelledonneCommunications/linphone-iphone"
  s.license      = { :type => "GNU GPL 3", :text => <<-LICENSE
@LINPHONESDK_LICENSE@LICENSE
    }
  s.author       = { 'Belledonne Communications SARL' => 'linphone-iphone@belledonne-communications.com' }
  s.platform     = :ios, "13.0"
  s.source       = { :http => "@LINPHONESDK_IOS_BASE_URL@/@LINPHONESDK_NAME@-@LINPHONESDK_VERSION@.zip" }
  s.vendored_frameworks = "@LINPHONESDK_NAME@/apple-darwin/@LINPHONESDK_FRAMEWORK_FOLDER@/**"
  s.pod_target_xcconfig = { 'VALID_ARCHS' => "@VALID_ARCHS@" }
  s.user_target_xcconfig = { 'VALID_ARCHS' => "@VALID_ARCHS@" }
  s.module_name   = 'linphonesw' # name of the swift package
  s.swift_version = '4.0'
  s.source_files = "@LINPHONESDK_NAME@/apple-darwin/share/linphonesw/*.swift"
  s.framework = 'linphone', 'belle-sip', 'bctoolbox'


end
