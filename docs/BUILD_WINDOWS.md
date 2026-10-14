# Windows builds

Requirements: x64 Windows 10 build 19041 or newer, CMake 3.22+, Visual Studio
C++20 compiler and Desktop development with C++ workload, Windows SDK, Git.
The supplied build used Visual Studio 2026 / MSVC 19.50.35724 and SDK 10.0.26100.0.
The iOS app uses a separate Xcode or xtool toolchain.

From the repository root:

```powershell
./scripts/build_windows.ps1 -BuildDir ./build
```

JUCE is pinned to 8.0.6 / commit 51a8a6d7aeae7326956d747737ccf1575e61e209. Network
is needed only to obtain dependencies; normal streaming is entirely local.
To use a separate local checkout, pass `-JuceDir <path-to-JUCE>`.

Equivalent explicit commands for the Visual Studio version used here:

```powershell
cmake -S . -B build -G 'Visual Studio 18 2026' -A x64
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

Other installed Visual Studio generators may work, but were not tested. CMake
uses the static MSVC runtime; no VC++ runtime redistributable installer is needed.
JUCE carries its required VST3 SDK sources and licenses.

Build targets: LiveLinkPlugin_VST3, LiveLinkDesktop, LiveLinkService,
LiveLinkTests, LiveLinkReceiverTests, LiveLinkPluginTests, LiveLinkProbe,
LiveLinkVSTHostTest, LiveLinkNetworkStress. The last five are test tools, not
production UI. Software receivers identify themselves with TEST labels.

```powershell
./scripts/run_tests.ps1 -BuildDir ./build -Integration
./build/Release/LiveLinkVSTHostTest.exe './build/LiveLinkPlugin_artefacts/Release/VST3/Live Link.vst3'
```

The integration runner starts and gracefully stops its own background process.
Close your DAW and existing Live Link background process before these tests;
the processor test owns the per-user mapping. Do not run multiple test runners
simultaneously. NetworkStress must run with a service started and defaults of
128 network frames / all interfaces; it uses real UDP loopback sockets.

Install Inno Setup 6.7.3 or a compatible 6.x compiler for packaging:

```powershell
./scripts/package_windows.ps1 -BuildDir ./build -ISCC 'C:/Program Files (x86)/Inno Setup 6/ISCC.exe'
```

Output: `dist/LiveLinkSetup.exe` and `dist/windows/`. The installer needs elevation
for Program Files, the standard VST3 path and its private-LAN firewall rule.
Sign the installer and production binaries before release. This distribution is
unsigned. Release portability and install/uninstall must be verified on clean
machines as described in TESTING.md.
