# Uncomment the next line to define a global platform for your project
platform :ios, '9.0'

target 'LinphoneTester' do
  # Uncomment the next line if you're using Swift or would like to use dynamic frameworks
  use_frameworks!

  # Pods for LinphoneTester
  pod 'linphone-sdk/all-frameworks', :path => "@LINPHONESDK_BUILD_DIR@/"

  target 'LinphoneTesterTests' do
    inherit! :search_paths
    # Pods for testing
  end

end
