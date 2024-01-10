Pod::Spec.new do |s|

  s.name         = "@LINPHONESDK_NAME@"
  s.version      = "@LINPHONESDK_VERSION@"
  s.summary      = "This is the linphone SDK, a free (GPL) video voip library based on the SIP protocol"
  s.description  = <<-PODSPEC_DESC
@LINPHONESDK_ENABLED_FEATURES@PODSPEC_DESC
  s.homepage     = "https://github.com/BelledonneCommunications/linphone-desktop"
  s.license      = { :type => "GNU GPL 3", :text => <<-LICENSE
@LINPHONESDK_LICENSE@LICENSE
    }
  s.author       = { 'Belledonne Communications SARL' => 'linphone-desktop@belledonne-communications.com' }
  s.platform     = :osx, "10.15"
  s.source       = { :http => "@LINPHONESDK_MACOS_BASE_URL@/@LINPHONESDK_NAME@-@LINPHONESDK_PLATFORM@-@LINPHONESDK_VERSION@.zip" }
  s.vendored_frameworks = "@LINPHONESDK_NAME@/@LINPHONESDK_PLATFORM@/@LINPHONESDK_FRAMEWORK_FOLDER@/**"
  s.pod_target_xcconfig = { 'VALID_ARCHS' => "@VALID_ARCHS@" }
  s.source_files = "@LINPHONESDK_NAME@/macos/share/linphonesw/*.swift"
  s.module_name   = 'linphonesw' # name of the swift package
  s.swift_version = '4.0'
  s.frameworks = 'linphone', 'belle-sip', 'bctoolbox'

  s.subspec 'all-frameworks' do |sp|
	sp.vendored_frameworks = "@LINPHONESDK_NAME@/macos/@LINPHONESDK_FRAMEWORK_FOLDER@/**"
  end
 
end
