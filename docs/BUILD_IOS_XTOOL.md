# iOS build with xtool

The native SwiftUI application and Objective-C++ audio receiver compiled in
Release mode through xtool 1.21.0, Swift 6.4 and a configured Darwin Swift SDK
inside Ubuntu 24.04 on WSL2. The target is ARM64 iOS 16 or newer.

Install and configure xtool and its SDK according to the
[official instructions](https://github.com/xtool-org/xtool).
From the repository root:

```sh
bash scripts/build_ios_xtool.sh
```

Equivalent command:

```sh
xtool dev build --configuration release --ipa
```

Output: `xtool/LiveLink.ipa`. The bundle identifier is `audio.livelink.receiver`.
The root `Package.swift` defines the Swift application and the LLAudio bridge.
`xtool.yml` supplies application metadata, icon and license resources.
USB laboratory sources are excluded from the application target.

## Signing

The default archive is unsigned. A standard iPhone requires valid signing and
provisioning. Authenticate and configure your developer identity locally using
xtool's documented setup, then build with:

```sh
xtool dev build --configuration release --ipa --sign
```

The signing workflow has not been independently validated for this release.
Do not commit account credentials, certificates or provisioning profiles.

## Validation

The generated archive passed ZIP integrity checks. Its executable is ARM64
Mach-O for iOS devices, with a minimum deployment version of 16.0. Application
identity, Bonjour declarations, local-network permission, background audio,
icon and framework dependencies were checked. The unsigned artifact contained
no provisioning profile or code-signature load command.

Local-network playback on a physical iPhone has subsequently been confirmed by
a user. Broader hardware acceptance remains in [TESTING.md](TESTING.md).
