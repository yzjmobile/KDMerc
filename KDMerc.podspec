#
# Be sure to run `pod lib lint KDMerc.podspec' to ensure this is a
# valid spec before submitting.
#
# Any lines starting with a # are optional, but their use is encouraged
# To learn more about a Podspec see http://guides.cocoapods.org/syntax/podspec.html
#

Pod::Spec.new do |s|
  s.name             = 'KDMerc'
  s.version          = '0.3.4'
  s.summary          = '基于mars封装的通讯层和日志层'

# This description is used to generate tags and improve search results.
#   * Think: What does it do? Why did you write it? What is the focus?
#   * Try to keep it short, snappy and to the point.
#   * Write the description between the DESC delimiters below.
#   * Finally, don't worry about the indent, CocoaPods strips it!

  s.description      = <<-DESC
基于微信mars开发，使用微信mars的stn、xlog组件，做了一层oc的封装。
                       DESC

  s.homepage         = 'https://github.com/yzjmobile/KDMerc'
  # s.screenshots     = 'www.example.com/screenshots_1', 'www.example.com/screenshots_2'
  s.license          = { :type => 'MIT', :file => 'LICENSE' }
  s.author           = { 'quding' => 'quding@163.com' }
  s.source           = { :git => 'git@github.com:yzjmobile/KDMerc.git', :tag => s.version.to_s }
  # s.social_media_url = 'https://twitter.com/<TWITTER_USERNAME>'

  s.ios.deployment_target = '8.0'
  s.static_framework = true

  s.frameworks = 'Foundation', 'CoreTelephony', 'SystemConfiguration'
  s.libraries = 'z', 'resolv.9', 'c++'

  s.source_files = 'KDMerc/*.{h,m,mm,cc}'
  s.public_header_files = 'KDMerc/*.h'

  s.subspec 'Log' do |ss|
    ss.source_files = 'KDMerc/Log/*.{h,m,mm,cc,swift}'
    ss.dependency 'KDMerc/Mars'
  end

  s.subspec 'Merc' do |ss|
    ss.source_files = 'KDMerc/Merc/*.{h,m,mm,cc}'
    ss.public_header_files = 'KDMerc/Merc/*.h'
    ss.private_header_files = 'KDMerc/Merc/stn_callback.h', 'KDMerc/Merc/app_callback.h'
    ss.dependency 'KDMerc/Mars'
  end

  s.subspec 'Mars' do |ss|
    ss.source_files = 'KDMerc/Mars/*.{h,m,mm,cc}'
    ss.private_header_files = 'KDMerc/Mars/longlink_packer.h', 'KDMerc/Mars/shortlink_packer.h', 'KDMerc/Mars/stnproto_logic.h'
    ss.vendored_frameworks = 'KDMerc/Mars/mars.framework'
  end

end
