# Native iOS builds

The app has now been compiled successfully through the installed xtool in WSL
and packaged as an unsigned ARM64 IPA. See [BUILD_IOS_XTOOL.md](BUILD_IOS_XTOOL.md)
for the verified Windows route. The Xcode route below remains untested and
requires macOS. Initial local-network playback on a physical iPhone has been confirmed by a user.

This repository contains a real `ios/LiveLink.xcodeproj/project.pbxproj`, a shared
scheme, SwiftUI sources, plist, app icon, Objective-C++ RemoteIO bridge and the
same C++ receiver tested on Windows. It is **not** a WebView application.
Xcode itself requires macOS. The separate xtool cross-build succeeded on Windows/WSL;
Xcode builds remain unverified.

1. Copy the complete source repository to a Mac with current Xcode and an iOS
   SDK supporting deployment target iOS 16.
2. Open `ios/LiveLink.xcodeproj`. Select the LiveLink target.
3. Set your development team and a bundle identifier you own. The default
   `audio.livelink.receiver` is a placeholder identifier, not a registered team.
4. Select the LiveLink scheme and build for a physical iPhone. Trust the Mac and
   enable Developer Mode as required by the phone's OS.
5. Confirm local network permission when browsing/connecting. Plist declares
   `_livelink._tcp` and `NSLocalNetworkUsageDescription`.
6. Accept the app's audio background mode and validate that playback survives
   locking, interruptions and route changes under supported OS conditions.

Simulator compile check on the Mac:

```sh
xcodebuild -project ios/LiveLink.xcodeproj -scheme LiveLink \
  -sdk iphonesimulator -configuration Debug CODE_SIGNING_ALLOWED=NO build
```

Signing/device build (substitute your real team ID):

```sh
xcodebuild -project ios/LiveLink.xcodeproj -scheme LiveLink \
  -destination 'generic/platform=iOS' DEVELOPMENT_TEAM=YOUR_TEAM build
```

`scripts/generate_xcode.py` regenerates the checked-in project without external
packages. `scripts/generate_icons.ps1` regenerates the Windows/iOS icon assets on
Windows. The C++ header search path must include the repository root; keep the
`shared/` folder beside `ios/`. C++20 is required. Swift language mode is 5.

No microphone permission is required: this application only plays received
audio. AVAudioSession uses `.playback`, requests 48 kHz and a 5 ms I/O duration,
then configures RemoteIO for the actual negotiated device rate. Requests are
preferences rather than latency guarantees. AirPlay/Bluetooth routes can add
substantial output delay; route behavior must be measured.

Interruption and route handlers are implemented; dedicated hardware acceptance tests remain pending.
The app is not submitted to App Store. Review network security, audio behavior,
accessibility, provisioning and distribution terms before shipping.
