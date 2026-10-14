// Isolated laboratory echo endpoint. Not connected to production transport/UI.
// Add this file to a dedicated Xcode test target and call start() on a real phone.
import Network
final class USBProbe {
    private var listener: NWListener?
    private var peers: [NWConnection] = []
    private let queue = DispatchQueue(label:"LiveLink.USBProof")
    func start() throws {
        let listener = try NWListener(using:.tcp,on:49322)
        listener.newConnectionHandler = { [weak self] peer in
            guard let self, self.peers.count < 4 else { peer.cancel(); return }
            self.peers.append(peer); peer.start(queue:self.queue); self.echo(peer)
        }
        listener.start(queue:queue); self.listener = listener
    }
    private func echo(_ peer: NWConnection) {
        peer.receive(minimumIncompleteLength:1,maximumLength:4096) { [weak self] data,_,complete,error in
            if let data { peer.send(content:data,completion:.contentProcessed { _ in }) }
            if complete || error != nil { peer.cancel(); self?.peers.removeAll { $0 === peer } } else { self?.echo(peer) }
        }
    }
    func stop() { listener?.cancel(); peers.forEach { $0.cancel() }; peers.removeAll() }
    deinit { stop() }
}
