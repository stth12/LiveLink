# Live Link

Stream stereo audio from a Windows DAW to an iPhone over your local network.
Live Link includes a VST3 plugin, a Windows controller and background process,
and a native iOS receiver.

## Status

Version 0.1.0 is an early release. Windows builds and automated tests have
passed. Local-network connection and audible playback on a physical iPhone
have been confirmed by a user. This is an initial functional check, not a
compatibility or latency benchmark. USB streaming is not implemented.

## Features

- Stereo VST3 pass-through with independent network monitoring.
- Up to 16 receiver sessions with individual volume, mute and mono controls.
- Bonjour discovery, manual IPv4 connection and an eight-digit pairing PIN.
- UDP PCM streaming with packet reordering, adaptive buffering and drift correction.
- Native SwiftUI receiver with RemoteIO playback and background audio support.
- Windows installer with optional startup and private-network firewall configuration.

## Quick start

1. Build and install the Windows components using [BUILD_WINDOWS.md](docs/BUILD_WINDOWS.md).
2. Build, sign and install the iOS receiver using [xtool](docs/BUILD_IOS_XTOOL.md)
   or [Xcode](docs/BUILD_IOS.md).
3. Connect the computer and iPhone to the same trusted local network.
4. Open Live Link on Windows. On iPhone, allow local-network access, select the
   computer and enter its pairing PIN. Manual IPv4 entry is available if discovery fails.
5. Insert Live Link on the stereo master bus in your DAW and start playback.

See [INSTALLATION.md](docs/INSTALLATION.md) for setup and troubleshooting.

## Build

Windows requires the Visual Studio C++ desktop workload, Windows SDK, CMake and Git.
JUCE 8.0.6 is fetched by CMake; it is not vendored in this repository.

```powershell
./scripts/build_windows.ps1
./scripts/run_tests.ps1 -Integration
./scripts/package_windows.ps1 -ISCC 'C:/Program Files (x86)/Inno Setup 6/ISCC.exe'
```

For iOS with a configured xtool and Darwin Swift SDK:

```sh
bash scripts/build_ios_xtool.sh
```

The resulting IPA is unsigned by default. Signing and provisioning are required
for installation on a standard iPhone. Build outputs are excluded from Git;
distribute installers and IPA files through GitHub Releases.

## Documentation

- [Architecture](docs/ARCHITECTURE.md) and [audio protocol](docs/AUDIO_PROTOCOL.md)
- [Validation status](docs/BUILD_RESULTS.md) and [test plan](docs/TESTING.md)
- [USB research](docs/USB_RESEARCH.md)
- [Contributing](CONTRIBUTING.md) and [security](SECURITY.md)
- [Publishing checklist](docs/PUBLISHING.md)

## Limitations

Use trusted local networks: control messages and audio are not encrypted.
The current transport is IPv4 only. One plugin owns capture per Windows user.
End-to-end latency, synchronized playback, broad DAW compatibility and long-running
hardware behavior have not been measured. Sample-rate conversion uses linear
interpolation. USB remains unavailable.

## License

AGPL-3.0-or-later. See [LICENSE](LICENSE), [COPYING](COPYING) and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). JUCE uses its AGPL licensing option;
proprietary distribution requires appropriate licensing.
