Pod::Spec.new do |s|

  s.name         = "linphone-sdk"
  s.version      = "@LINPHONESDK_VERSION@"
  s.summary      = "Liblinphone is a library to create VoIP and IM apps, based on SIP protocol."
  s.description  = <<-PODSPEC_DESC
@LINPHONESDK_ENABLED_FEATURES@PODSPEC_DESC
  s.homepage     = "https://github.com/BelledonneCommunications/linphone-iphone"
  s.license      = { :type => "GNU GPL 3", :text => <<-LICENSE
@LINPHONESDK_LICENSE@LICENSE
    }
  s.author       = { 'Belledonne Communications SARL' => 'linphone-iphone@belledonne-communications.com' }
  s.platform     = :ios, "9.0"
  s.source       = { :http => "@LINPHONESDK_IOS_BASE_URL@linphone-sdk-ios-@LINPHONESDK_VERSION@.zip" }
  s.vendored_frameworks = "linphone-sdk/apple-darwin/Frameworks/**"
  s.pod_target_xcconfig = { 'VALID_ARCHS' => "@VALID_ARCHS@" }
  s.module_name   = 'linphonesw' # name of the swift package
  s.swift_version = '4.0'

  s.subspec 'all-frameworks' do |sp|
    sp.vendored_frameworks = "linphone-sdk/apple-darwin/Frameworks/**"
  end

  s.subspec 'basic-frameworks' do |sp|
    sp.dependency 'linphone-sdk/app-extension'
    sp.vendored_frameworks = "linphone-sdk/apple-darwin/Frameworks/{@LINPHONE_OTHER_FRAMEWORKS@}"
  end

  s.subspec 'app-extension' do |sp|
    sp.vendored_frameworks = "linphone-sdk/apple-darwin/Frameworks/{@LINPHONE_APP_EXT_FRAMEWORKS@}"
  end

  s.subspec 'app-extension-swift' do |sp|
    sp.source_files = "linphone-sdk/apple-darwin/share/linphonesw/*.swift"
    sp.dependency "linphone-sdk/app-extension"
    sp.framework = 'linphone', 'belle-sip', 'bctoolbox'
  end

  s.subspec 'swift' do |sp|
    sp.dependency "linphone-sdk/basic-frameworks"
    sp.dependency "linphone-sdk/app-extension-swift"
    sp.framework = 'bctoolbox-ios'
  end

end
