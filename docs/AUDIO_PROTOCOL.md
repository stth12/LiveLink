# Live Link protocol 1

All integers and IEEE-754 binary32 samples are serialized in network byte order
(big endian). No compiler struct or native padding is transmitted. UDP payload
size is `48 + frames * 2 * 4`, with frames 32, 64 or 128. Maximum payload is 1072
bytes, below a typical Ethernet MTU after IP/UDP headers.

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 4 | Magic `0x4C4C4E4B` (LLNK) |
| 4 | 4 | Version = 1 |
| 8 | 8 | Random authorized session token |
| 16 | 4 | Stream identifier |
| 20 | 4 | Packet sequence, modulo 2^32 |
| 24 | 8 | Timestamp: source sample frame index of first payload frame |
| 32 | 4 | Source sample rate Hz (8000–192000 accepted) |
| 36 | 4 | Frame count, 32 / 64 / 128 |
| 40 | 4 | Channel count = 2 |
| 44 | 4 | Reserved flags = 0 |
| 48 | frames × 8 | Interleaved L,R float32 PCM |

Non-finite source samples are replaced with zero in the network copy. DAW
pass-through is unchanged. The receiver rejects invalid magic, versions, size,
channel count, reserved flags, sample rate, token and non-finite samples.
Sequence comparisons use signed modulo differences within the reorder window.
Stream or rate changes reset playback priming. Timestamps describe the DAW clock,
not wall-clock time and not an acoustic playback deadline.

The reliable control channel is TCP IPv4 port 49320. Each UTF-8 JSON object ends
in one LF and is at most 8192 bytes. Unknown/unauthorized requests return
`{"ok":false}`. Clients send a ping at least every 5 seconds; the supplied iOS
client pings every 500 ms. An inactive session is removed after 10 seconds.

Receiver pairing request:

```json
{"op":"pair","pin":"12345678","port":55000,"name":"My iPhone"}
```

The PIN is a newly generated 8-digit number shown by the desktop. Five failed
attempts lock pairing globally for 30 seconds. This can cause a local-network
denial of service; hardening should use per-peer limits too. A successful reply
contains `ok`, a 16-digit-or-shorter hexadecimal `token`, `version`, `frames`
(capture block size = 128), `mode` and the `autoReconnect` default. Actual UDP
packet sizes are self-describing and may differ from the capture block size.
The token authenticates received packet membership; it is not encryption or a
message authentication code. Credentials are volatile and do not persist.

Authenticated receiver requests:

```json
{"op":"device","volume":0.8,"mute":false,"mono":false,"mode":1}
{"op":"ping","loss":0,"underruns":0,"occupancy":960,"jitterMs":0.5}
{"op":"disconnect"}
```

Modes are 0 ultra low, 1 balanced, 2 stable. Gain is clamped 0–1. `connected:false`
pauses distribution while retaining the reliable session, and `true` resumes it.
Controls returned on ping allow desktop changes to reach the receiver UI.
Loss, underrun and jitter telemetry come from the receiver, not fabricated
service-side estimates. `bufferEstimateMs` is queued source frames / sample rate,
excluding all other latency.
Status includes `hasTelemetry`; clients must treat metrics as unknown while it
is false. The desktop displays “Receiver diagnostics pending” in that state.

Desktop/plugin-user administration is accepted only from 127.0.0.1 and only
after `{"op":"admin","token":"<shared-memory-admin-token-hex>"}`. The token is
random and separate from pairing/audio tokens. LAN callers cannot read it from
the service. Administrative operations are `status`, `rotatePin`, `shutdown`,
`device` with `id`, `all`, and `settings`. Settings fields are `defaultMode`,
`packetFrames`, `bindAddress`, `autoReconnect`, `logging`. All receiver identifiers
are ephemeral session identifiers. New PINs affect future connections; rotate
PIN does not revoke already authorized sessions. Process shutdown does.

Security scope: trusted private LAN, no cloud or mandatory internet. Control
and PCM are plaintext. TLS with paired certificate pinning and authenticated
encryption of PCM are required before use on hostile or shared networks.
