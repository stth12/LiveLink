# Testing and hardware acceptance

## Automated tests

- CoreTests: endian protocol round trip, malformed packets, 32/64/128 packet
  lengths, gain/mono/mute, ring capacity/drop behavior, unsigned index wrap and
  100,000 concurrently transferred blocks.
- ReceiverTests: session tokens, four source rates, actual SRC/render output,
  bounded reorder, gap concealment, stale packet rejection, priming and underrun
  silence, concurrent producer/render operations. Timing is a CPU test, not
  physical audio latency.
- PluginTests: actual compiled JUCE processor, bit-identical stereo pass-through,
  irregular host buffers, four sample rates, shared-memory capture, service
  offline behavior and producer ownership cleanup.
- VSTHostTest: loads the actual generated VST3 bundle in JUCE's VST3 host,
  instantiates its processor/editor and verifies pass-through at four rates.
  This is not a FL Studio / Ableton / REAPER / Cubase / Studio One test.
- Probe: five authenticated real TCP/UDP software clients, real mapping/service
  distribution, bad PIN rejection, one-device disconnect and independent mute.
- NetworkStress: 10 seconds of real-time-paced 48 kHz capture to five software
  receivers; one deliberately never reads UDP. Four receivers validate tokens,
  stream and sequence while the fifth socket fills. Results are loopback only.

Use scripts/run_tests.ps1 and BUILD_WINDOWS.md. Run processor/core tests before
starting the service. Software receivers are explicitly identified as tests and
are never substituted for production UI devices.

## Windows / iOS release gate

For each target DAW, record DAW version, OS, sample rate, buffer size, CPU load,
plugin scan/load result and offline/online playback. Run a bitwise output null
test with streaming enabled/disabled. Start/stop plugin UI, desktop and service
independently. Test multiple plugin instances and abrupt DAW exit.

On a Mac, compile the checked-in project first, fix all compiler errors, then
run on at least five physical iPhones. Record iOS versions, Wi-Fi chipset/AP,
network profile, route, buffer mode and source/output sample rates. Validate
discovery, manual address, PIN denial, pairing, audible PCM playback, controls,
five-device playback, screen lock, foreground/background, calls, interruption
resume policy, wired/Bluetooth route changes and app termination.

Run at least 60 minutes at 44.1/48/88.2/96 kHz. Introduce controlled loss/jitter
and disconnect one phone. Record audible glitches, occupancy, real loss,
underruns, drift correction, CPU, memory and thermal state. Packet metrics alone
do not prove audible continuity. Compare low-frequency and near-Nyquist sweeps
to characterize the current linear SRC and decide whether to replace it with
a band-limited implementation.

Verify fresh install / upgrade / uninstall on clean Windows machines with
same-account UAC elevation and separately with standard-user admin credentials.
Check VST3 folder, desktop files, private-LAN-only firewall rule, optional startup,
normal user privileges, stopped process and clean removal. Silent uninstall
must retain preferences; interactive deletion must remove only named Live Link
files. Test uninstallation with the DAW/plugin still loaded and decline unsafe
forced close/reboot behavior. Verify binary signing before general distribution.

## Acoustic latency method

Use a 2-channel measurement interface. Split the DAW's click/chirp source into a
direct electrical reference recording and the Live Link stream. Place a calibrated
measurement microphone a measured distance from one phone speaker and record it
on channel 2. Capture both signals on the **same ADC clock**, avoiding two
unrelated recording clocks. Identify acoustic output with cross-correlation and
manually inspect the impulse response. Subtract microphone propagation delay
(distance divided by measured/appropriate sound speed) and calibrate unequal
input-chain delays with a loopback. Repeat at least 100 clicks under idle and
loaded network conditions; report median, p95, min/max and confidence bounds.

State the phone, output route, source rate, I/O buffer, jitter mode, AP and
distance. A queue occupancy estimate is not this measurement. For multi-phone
skew use simultaneous microphones into the same multichannel recorder, with
each speaker/mic distance calibrated. Do not infer sample-accurate acoustic
synchronization from sequence/timestamp equality.

The 20–40 ms objective has not been acoustically measured here. Treat all
end-to-end and synchronized playback claims as unverified until these tests pass.
