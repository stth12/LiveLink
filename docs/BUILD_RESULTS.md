# Validation status — 0.1.0

## Verified

- Windows Release builds of the desktop, background process and VST3 plugin.
- Core, receiver and actual JUCE processor tests: three suites passed.
- VST3 loading and bit-identical stereo pass-through in a JUCE test host.
- Five authenticated software receivers over real loopback TCP/UDP sockets.
- Independent volume, mute, mono, disconnect and packet-size controls.
- Ten-second loopback stress test with one receiver deliberately not consuming UDP;
  the other four received 3,750 packets each without sequence gaps.
- Windows installer installation/uninstallation and associated startup/firewall cleanup
  in a same-account test using an isolated application installation directory.
- Native SwiftUI and Objective-C++ receiver compilation through xtool in Release mode;
  unsigned ARM64 iOS archive integrity and application metadata checks.
- User-reported local-network connection and audible playback on a physical iPhone.
  Device model, iOS version, DAW version and acoustic latency were not recorded.

## Remaining validation

Commercial DAW compatibility across versions, clean-machine deployment, multiple
physical receivers, extended playback, interruption/route recovery, background
behavior, acoustic latency and audio quality require further hardware testing.
Xcode builds and the xtool signing workflow have not been independently validated.

USB is not implemented. No USB, App Store, production-security or synchronized
acoustic-playback support is claimed.
