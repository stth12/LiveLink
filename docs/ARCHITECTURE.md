# Architecture

The DAW callback reads stereo input, writes a 128-frame interleaved capture block
to shared memory and leaves input/output bits unchanged. The GUI has no role in
capture. The plugin's maintenance thread opens the map, checks the heartbeat
and claims producer ownership. The callback performs no locks, allocation,
networking, filesystem access or OS calls. Partial blocks survive ordinary DAW
block boundaries; a capture state transition drops the partial block and starts
a new stream identifier. No audio processing is inserted into the DAW output.

`SharedMemory.h` defines a 256-slot SPSC ring. Producer writes payload before
release-publishing its write index; service acquire-loads that index before
reading, then release-publishes its read index. Full rings drop new capture
blocks, count the drop and never block the DAW. Unsigned indices support wrap.
The mapping is named with the user's SID in the Local namespace and has the
process's default Windows DACL. Only lock-free integer atomics are used in IPC.
This ABI is Windows x64 / MSVC-specific and version checked; it is **not** the
cross-platform network protocol. One plugin instance owns capture at a time.

The background process has its own per-user singleton mutex, shared-memory
lifetime and UDP distribution thread. It continues after the desktop closes.
A crashed producer is detected by its process handle and released. Meters,
counts and receiver names are atomic; names use an atomic revision snapshot.
Receiver controls are atomics, updated outside the distribution loop. Each
receiver gets an independently encoded volume/mono/mute packet. UDP sockets are
nonblocking, so a stopped consumer cannot back-pressure other receivers.

The control server uses bounded messages (8 KiB), at most 32 concurrent control
sessions, at most 16 receivers, 1-second read/send timeouts and a 10-second
inactivity deadline. Completed session threads are reaped. Control peers must
authenticate before streaming. The UDP destination uses the authenticated TCP
peer's IPv4 address and a validated receiver port; callers cannot select an
arbitrary third-party IP address.

Desktop I/O runs on its own worker. UI code reads status snapshots and queues
commands. Network waits never run on the GUI timer or the DAW callback. The
desktop launches the background process using `CREATE_NO_WINDOW`. Startup uses
a small Windows Script Host launcher to avoid a console window. A Windows
Service is intentionally not used: audio capture, pairing and administration
belong to the interactive user rather than privileged Session 0.

The iOS network queue owns packet parsing, reordering and producer writes. The
RemoteIO callback owns ring reads, fractional sample position and priming state.
Its C++ implementation does not allocate, lock, call Swift or perform network
I/O. The network side has 32 reorder slots, ignores duplicate/late packets,
conceals a gap after three successor packets and resets on a stream/rate change.
The 32,768-frame render ring drops new samples on overflow. Atomics publish
indices and telemetry. UI updates are dispatched to the main queue.

Base jitter targets are 8 / 20 / 40 ms, with up to 40 ms extra based on a smoothed
arrival variation estimate. Occupancy feedback changes the SRC ratio by at most
±0.2%. Linear interpolation converts the negotiated source rate to the actual
AVAudioSession device rate. Underruns produce silence and re-prime; the beginning
of resumed playback ramps in. The heuristics and resampler need device tuning.
No sample-accurate synchronization among speakers is claimed.

Windows registers `_livelink._tcp.local` through DNS-SD. Registration status is
reported rather than assumed successful. A selected interface restricts LAN
control binding, UDP source binding and discovery interface. A separate loopback
admin listener remains available. The default is all interfaces. Network
interface changes require process restart. WiFiTransport implements real UDP;
USBTransport explicitly reports unavailable and never reports connected.

Future synchronized mode needs a monotonic clock exchange, clock-offset bounds,
scheduled render timestamps, calibrated output-route delays and measured drift.
The existing stream identifiers and source sample timestamps are prerequisites,
not evidence that acoustic synchronization already works.
