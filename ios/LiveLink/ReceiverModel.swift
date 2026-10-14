import Foundation
import Network
import AVFAudio
import UIKit
import Combine
#if canImport(LLAudio)
import LLAudio
#endif

final class ReceiverModel: ObservableObject {
    @Published var computers: [NWBrowser.Result] = []
    @Published var status = "DISCONNECTED"
    @Published var error = ""
    @Published var computer = ""
    @Published var volume: Double = 1
    @Published var muted = false
    @Published var mono = false
    @Published var mode = 1
    @Published var autoReconnect = true
    @Published var rate: UInt32 = 0
    @Published var peak: Float = 0
    @Published var occupancy: UInt32 = 0
    @Published var loss: UInt32 = 0
    @Published var underruns: UInt32 = 0
    @Published var jitterMs: Double = 0
    private let queue = DispatchQueue(label: "LiveLink.network", qos: .userInitiated)
    private let engine = ll_create()!
    private var browser: NWBrowser?
    private var tcp: NWConnection?
    private var udp: NWListener?
    private var udpConnections: [NWConnection] = []
    private var timer: DispatchSourceTimer?
    private var pending = Data()
    private var authorized = false
    private var endpoint: NWEndpoint?
    private var pairingPIN = ""
    private var lastPacket: UInt64 = 0
    private var lastArrival: UInt64 = 0
    private var jitter: Double = 0
    private var generation = 0
    private var wantsConnection = false
    private var reconnect = true
    private var paused = false
    private var audioSuspended = false
    init() {
        browse()
        NotificationCenter.default.addObserver(self, selector: #selector(interruption(_:)), name: AVAudioSession.interruptionNotification, object: nil)
        NotificationCenter.default.addObserver(self, selector: #selector(routeChanged), name: AVAudioSession.routeChangeNotification, object: nil)
    }
    deinit { timer?.cancel(); browser?.cancel(); tcp?.cancel(); udp?.cancel(); udpConnections.forEach { $0.cancel() }; ll_destroy(engine); NotificationCenter.default.removeObserver(self) }
    private func publish(_ action: @escaping () -> Void) { DispatchQueue.main.async(execute: action) }
    func browse() {
        let parameters = NWParameters.tcp
        parameters.includePeerToPeer = false
        let b = NWBrowser(for: .bonjour(type: "_livelink._tcp", domain: nil), using: parameters)
        b.browseResultsChangedHandler = { [weak self] results, _ in self?.publish { self?.computers = Array(results).sorted { String(describing: $0.endpoint) < String(describing: $1.endpoint) } } }
        b.stateUpdateHandler = { [weak self] state in if case .failed(let e) = state { self?.publish { self?.error = "Discovery: \(e.localizedDescription). You can enter an IP address." } } }
        b.start(queue: queue); browser = b
    }
    func displayName(for endpoint: NWEndpoint) -> String {
        if case .service(let name, _, _, _) = endpoint { return name }
        return "Windows computer"
    }
    func connect(to selected: NWEndpoint, pin: String, name: String) {
        let reconnectPreference = autoReconnect
        queue.async { [weak self] in guard let self else { return }; self.wantsConnection = true; self.reconnect = reconnectPreference; self.endpoint = selected; self.pairingPIN = pin; self.publish { self.computer = name; self.error = "" }; self.beginConnection() }
    }
    func manualConnect(host: String, pin: String) { connect(to: .hostPort(host: NWEndpoint.Host(host), port: 49320), pin: pin, name: host) }
    private func beginConnection() {
        closeTransport(); generation += 1; let attempt = generation
        guard wantsConnection, let endpoint else { return }
        publish { self.status = "CONNECTING" }
        do {
            let listener = try NWListener(using: .udp, on: .any)
            listener.newConnectionHandler = { [weak self] connection in
                guard let self, attempt == self.generation, self.udpConnections.count < 8 else { connection.cancel(); return }
                self.udpConnections.append(connection); connection.start(queue: self.queue); self.receiveAudio(connection, attempt: attempt)
            }
            listener.stateUpdateHandler = { [weak self] state in
                guard let self, attempt == self.generation else { return }
                if case .ready = state, let port = listener.port { self.openControl(endpoint, port: port.rawValue, attempt: attempt) }
                if case .failed(let e) = state { self.failed("UDP: \(e.localizedDescription)", attempt: attempt) }
            }
            listener.start(queue: queue); udp = listener
        } catch { failed(error.localizedDescription, attempt: attempt) }
    }
    private func openControl(_ endpoint: NWEndpoint, port: UInt16, attempt: Int) {
        let connection = NWConnection(to: endpoint, using: .tcp); tcp = connection
        connection.stateUpdateHandler = { [weak self] state in
            guard let self, attempt == self.generation else { return }
            switch state {
            case .ready:
                self.send(["op":"pair", "pin":self.pairingPIN, "port":Int(port), "name":UIDevice.current.name]); self.receiveControl(connection, attempt: attempt)
            case .failed(let e): self.failed(e.localizedDescription, attempt: attempt)
            case .waiting(let e): self.publish { self.error = "Waiting for local network access: \(e.localizedDescription)" }
            default: break
            }
        }
        connection.start(queue: queue)
        queue.asyncAfter(deadline: .now() + 12) { [weak self] in guard let self, attempt == self.generation, !self.authorized else { return }; self.failed("Connection timed out. Check pairing PIN and firewall.", attempt: attempt) }
    }
    private func send(_ value: [String:Any]) { guard let bytes = try? JSONSerialization.data(withJSONObject: value) else { return }; var line = bytes; line.append(10); tcp?.send(content: line, completion: .contentProcessed { _ in }) }
    private func receiveControl(_ connection: NWConnection, attempt: Int) {
        connection.receive(minimumIncompleteLength: 1, maximumLength: 8192) { [weak self] data, _, complete, e in
            guard let self, attempt == self.generation else { return }
            if let data { self.pending.append(data) }
            if self.pending.count > 16384 { self.failed("Oversized control response", attempt: attempt); return }
            while let newline = self.pending.firstIndex(of: 10) {
                let line = self.pending[..<newline]; self.pending.removeSubrange(...newline)
                if let value = try? JSONSerialization.jsonObject(with: Data(line)) as? [String:Any] { self.handle(value, attempt: attempt) }
            }
            if complete || e != nil { self.failed(e?.localizedDescription ?? "Computer disconnected", attempt: attempt) } else { self.receiveControl(connection, attempt: attempt) }
        }
    }
    private func handle(_ value: [String:Any], attempt: Int) {
        if value["ok"] as? Bool != true { wantsConnection = false; failed(value["error"] as? String ?? "Pairing refused", attempt: attempt); return }
        if let hex = value["token"] as? String, let token = UInt64(hex, radix:16) {
            if let preference = value["autoReconnect"] as? Bool { reconnect = preference; publish { self.autoReconnect = preference } }; ll_authorize(engine,token); authorized = true; startAudio(); startTimer(); publish { self.status = "CONNECTED" }
        }
        if let connected = value["connected"] as? Bool { paused = !connected; if paused { publish { self.status = "PAUSED" } } }
        publish {
            if let v = value["volume"] as? Double { self.volume = v }
            if let v = value["mute"] as? Bool { self.muted = v }
            if let v = value["mono"] as? Bool { self.mono = v }
            if let v = value["mode"] as? Int { self.mode = v }
        }
        if let m = value["mode"] as? Int { ll_mode(engine,Int32(m)) }
    }
    private func receiveAudio(_ connection: NWConnection, attempt: Int) {
        connection.receiveMessage { [weak self] data, _, _, e in
            guard let self, attempt == self.generation else { return }
            if let data, self.authorized {
                let accepted = data.withUnsafeBytes { bytes in ll_packet(self.engine, bytes.bindMemory(to: UInt8.self).baseAddress, Int32(bytes.count)) }
                if accepted != 0 {
                    let now = DispatchTime.now().uptimeNanoseconds
                    if self.lastArrival != 0 { let interval = Double(now-self.lastArrival)/1_000_000; let frames = data[36..<40].reduce(UInt32(0)) { ($0 << 8) | UInt32($1) }; let expected = Double(frames)*1000 / Double(ll_rate(self.engine)); self.jitter += (abs(interval-expected)-self.jitter)/16 }
                    self.lastArrival = now; self.lastPacket = now
                }
            }
            if e == nil { self.receiveAudio(connection, attempt: attempt) } else { connection.cancel(); self.udpConnections.removeAll { $0 === connection } }
        }
    }
    private func startAudio() {
        do {
            let session = AVAudioSession.sharedInstance()
            try session.setCategory(.playback, mode: .default, options: [])
            try session.setPreferredSampleRate(48000); try session.setPreferredIOBufferDuration(0.005); try session.setActive(true)
            let result = ll_start(engine, session.sampleRate)
            audioSuspended = result != 0
            if result != 0 { publish { self.error = "Core Audio could not start (\(result))" } }
        } catch { audioSuspended = true; publish { self.error = "Audio session: \(error.localizedDescription)" } }
    }
    private func startTimer() {
        timer?.cancel(); let t = DispatchSource.makeTimerSource(queue: queue)
        t.schedule(deadline: .now(), repeating: .milliseconds(500))
        t.setEventHandler { [weak self] in
            guard let self else { return }
            let rate: UInt32 = self.lastPacket == 0 ? 0 : ll_rate(self.engine); let level = ll_peak(self.engine), occupancy = ll_occupancy(self.engine), loss = ll_loss(self.engine), underruns = ll_underruns(self.engine)
            let active = self.lastPacket != 0 && DispatchTime.now().uptimeNanoseconds-self.lastPacket < 1_000_000_000
            self.send(["op":"ping", "loss":loss, "underruns":underruns, "occupancy":occupancy, "jitterMs":self.jitter])
            let currentJitter = self.jitter; let currentStatus = self.paused || self.audioSuspended ? "PAUSED" : active ? "STREAMING" : "CONNECTED"; self.publish { self.rate = rate; self.peak = active ? level : 0; self.occupancy = occupancy; self.loss = loss; self.underruns = underruns; self.jitterMs = currentJitter; self.status = currentStatus }
        }
        t.resume(); timer = t
    }
    func controlsChanged() {
        let v = volume, m = muted, monoValue = self.mono, modeValue = self.mode, reconnectPreference = autoReconnect
        queue.async { [weak self] in guard let self else { return }; self.reconnect = reconnectPreference; self.send(["op":"device", "volume":v, "mute":m, "mono":monoValue, "mode":modeValue]); ll_mode(self.engine,Int32(modeValue)) }
    }
    func disconnect() { queue.async { [weak self] in guard let self else { return }; self.wantsConnection = false; self.send(["op":"disconnect"]); self.generation += 1; self.closeTransport(); self.publish { self.status = "DISCONNECTED" } } }
    func resumePlayback() { queue.async { [weak self] in guard let self, self.authorized else { return }; self.paused = false; self.startAudio(); self.send(["op":"device", "connected":true]) } }
    private func closeTransport() { timer?.cancel(); timer = nil; tcp?.cancel(); tcp = nil; udp?.cancel(); udp = nil; udpConnections.forEach { $0.cancel() }; udpConnections.removeAll(); pending.removeAll(); authorized = false; lastPacket = 0; lastArrival = 0; jitter = 0; ll_stop(engine) }
    private func failed(_ message: String, attempt: Int) {
        guard attempt == generation else { return }; generation += 1; let next = generation; closeTransport(); publish { self.error = message; self.status = self.wantsConnection && self.reconnect ? "RECONNECTING" : "DISCONNECTED" }
        if wantsConnection && reconnect { queue.asyncAfter(deadline: .now()+3) { [weak self] in guard let self, next == self.generation else { return }; self.beginConnection() } }
    }
    @objc private func interruption(_ note: Notification) {
        let type = (note.userInfo?[AVAudioSessionInterruptionTypeKey] as? NSNumber)?.uintValue
        let options = (note.userInfo?[AVAudioSessionInterruptionOptionKey] as? NSNumber)?.uintValue ?? 0
        queue.async { [weak self] in guard let self else { return }; if type == AVAudioSession.InterruptionType.began.rawValue { self.audioSuspended = true; ll_stop(self.engine); self.publish { self.status = "PAUSED" } } else if self.authorized && options & AVAudioSession.InterruptionOptions.shouldResume.rawValue != 0 { self.startAudio() } }
    }
    @objc private func routeChanged() { queue.async { [weak self] in guard let self, self.authorized else { return }; self.startAudio() } }
}
