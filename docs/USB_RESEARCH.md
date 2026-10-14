# USB: unavailable in the production transport

No iPhone, Apple Mobile Device driver stack or macOS is available for a USB
hardware proof here. The production USB adapter returns unavailable and never
shows Connected. No USB audio feature is claimed.

## Investigated mechanism

The libimobiledevice project's [libusbmuxd primary documentation](https://github.com/libimobiledevice/libusbmuxd)
describes a TCP forwarding client (`iproxy`) and says Windows uses the Apple
Mobile Device Support package for its multiplexing service. That is a plausible
host-to-device TCP path, not a generic USB UDP network adapter and not evidence
that this project's app has exchanged data over USB.

A normal native iOS app can create an NWListener TCP endpoint. The isolated
`ios/LiveLink/USBProbe.swift` listens on 49322 and echoes bytes. It is intentionally
excluded from the production Xcode target and has no production UI switch. The
Windows `scripts/usb_probe.ps1` sends a random nonce to a localhost TCP forwarder
and requires a complete, matching echo. Neither file creates an imaginary Apple
USB API. The bidirectional test has **not** been run through a physical cable.

## Required laboratory validation

1. Install the appropriate supported Apple Devices / Apple Mobile Device driver
   package from Apple and record its exact version and device drivers.
2. Obtain/build a legally distributable, reviewed Windows libusbmuxd/iproxy
   version. Check its help and port-forwarding arguments for that version.
3. Sign a separate laboratory iOS target containing USBProbe. Call `start()`
   while the app is foreground. Do not expose the test listener in a release.
4. Connect a normal, non-jailbroken phone by cable, unlock it and complete the
   Windows/device Trust pairing prompt. Record iPhone model and OS version.
5. Forward localhost TCP 49322 to device TCP 49322 using the verified tool.
6. Disable LAN/Wi-Fi access to rule out an accidental wireless route, and run
   `scripts/usb_probe.ps1`. Capture transport logs proving the cable path.
7. Repeat cable removal/reconnect, screen lock, foreground/background, driver
   restarts, multiple attached phones and iOS updates.
8. Only after that evidence, implement a framed USB TCP transport adapter with
   bounded receiver queues, independent client flow control and pairing.

Drivers and trust prompts are external requirements. Do not bundle Apple driver
components without checking redistribution terms. libusbmuxd's license must be
reviewed before bundling it or a static link. Apple Mobile Device's Windows
interfaces are not an Apple-supported public third-party streaming SDK; relying
on protocol reimplementation creates update/compatibility risks. App Store,
background listening and distribution acceptance must be investigated with
Apple guidance and actual submission results. App Store approval is not assumed.

Status: research-backed candidate + isolated test source; **hardware transport
unverified, production integration unavailable**.
