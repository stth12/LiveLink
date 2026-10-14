# Install and use

## Windows installer

1. Run `dist/LiveLinkSetup.exe` and approve Windows elevation.
2. Keep Desktop, Required Background Process and 64-bit VST3 selected.
3. Optionally select a desktop shortcut and Start with Windows.
4. The desktop and service install into `C:\Program Files\Live Link\`.
5. The plugin installs into
   `C:\Program Files\Common Files\VST3\Live Link.vst3\`.
6. Use a trusted private local network. The installer adds an inbound TCP 49320
   firewall rule restricted to private networks and local-subnet peers.
7. Launch Live Link. Its background process starts without administrator rights
   and remains running after closing the dashboard.
8. Rescan VST3 plugins in your DAW and insert Live Link on the stereo master bus.
   Leave your usual DAW audio output enabled. This plugin does not change it.

DAW scanning and playback must be tested in your specific DAW; no named DAW has
been validated in this environment. Only one plugin captures per Windows user.
The DAW, desktop and background process must use the same Windows account.

## Portable use

Keep `dist/windows/Live Link.exe` and `Live Link Service.exe` together. Launch
the desktop. Manually copy the entire `Live Link.vst3` bundle into the standard
VST3 folder if you want DAW scanning. Configure your private-network firewall to
allow inbound TCP 49320 for this exact background executable. Portable use does
not modify the firewall or register startup automatically.

## iPhone

Build, sign and install the native app using xtool (BUILD_IOS_XTOOL.md) or Xcode (BUILD_IOS.md). Connect the
iPhone and PC to the same trusted Wi-Fi/LAN. Open the receiver, allow local
network access, enter the desktop's 8-digit pairing PIN and select the computer.
If Bonjour discovery fails, enter the PC's IPv4 address and use Connect by
Address. The service defaults to all interfaces; selected-interface changes
require a service restart from Settings.

Start DAW playback. The iPhone should transition from CONNECTED to STREAMING
only after authorized audio packets arrive. Test actual audible output; this
transition is not proof of acoustic playback. Add additional iPhones the same
way. Volume, mono, mute and buffer controls affect the receiver network copy,
never the normal DAW output. Desktop Disconnect pauses that device's stream
while retaining its control session for Connect; iPhone Disconnect ends pairing.
CONNECT ALL resumes retained sessions; it cannot reconnect a closed phone app.

USB is explicitly unavailable. Buffer estimates are not end-to-end latency.

## Stop / uninstall

Closing the dashboard does not stop streaming. To stop the background process:

```powershell
& 'C:/Program Files/Live Link/Live Link Service.exe' --stop
```

Close your DAW and dashboard before uninstalling. Remove Live Link through
Windows Installed Apps. The uninstaller stops the per-user background process,
removes installed components, shortcuts, its startup value and its named
firewall rule. Interactive uninstall asks whether to keep preferences/logs;
silent uninstall retains them. Only Live Link's named files are deleted.
Pairing credentials are volatile and disappear on service exit.

Preferences: `%APPDATA%\Live Link\preferences.json`. Diagnostic logging, if
enabled, writes `%APPDATA%\Live Link\diagnostics.log`. The dashboard's Save
Diagnostics command writes a PIN-redacted status JSON to Documents/Live Link
Diagnostics. Test installations using a different admin account and all-user
startup semantics still require verification; the intended setup is same-account
elevation and per-user operation.
