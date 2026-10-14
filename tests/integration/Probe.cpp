#include "shared/ipc/SharedMemory.h"
#include "shared/transport/Sockets.h"
#include <cmath>
#include <iostream>
#include <juce_core/juce_core.h>
#include <thread>
static juce::var request(SOCKET s, juce::String text) {
    ll::sendText(s, text.toStdString() + "\n");
    return juce::JSON::parse(ll::readLine(s));
}
int main() {
    try {
        ll::Network net;
        ll::Mapping map(false);
        auto *shared = map.get();
        if (!shared) {
            std::cerr << "Start service first\n";
            return 1;
        }
        ll::Socket admin(ll::connectLocal());
        auto configuration = request(
            admin.s, "{\"op\":\"admin\",\"token\":\"" +
                         juce::String::toHexString((juce::int64)shared->adminToken.load()) + "\"}");
        unsigned packetFrames = unsigned(int(configuration["settings"]["packetFrames"]));
        if (packetFrames != 32 && packetFrames != 64 && packetFrames != 128)
            return 11;
        std::vector<std::unique_ptr<ll::Socket>> controls, receivers;
        std::vector<uint64_t> tokens;
        for (int i = 0; i < 5; ++i) {
            auto udp = std::make_unique<ll::Socket>(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
            sockaddr_in a{};
            a.sin_family = AF_INET;
            a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            if (bind(udp->s, (sockaddr *)&a, sizeof(a)))
                return 2;
            int n = sizeof(a);
            getsockname(udp->s, (sockaddr *)&a, &n);
            DWORD ms = 2000;
            setsockopt(udp->s, SOL_SOCKET, SO_RCVTIMEO, (char *)&ms, sizeof(ms));
            auto tcp = std::make_unique<ll::Socket>(ll::connectLocal());
            auto r =
                request(tcp->s, "{\"op\":\"pair\",\"pin\":\"" + juce::String(shared->pin.load()) +
                                    "\",\"port\":" + juce::String(ntohs(a.sin_port)) +
                                    ",\"name\":\"TEST RECEIVER " + juce::String(i) + "\"}");
            if (!bool(r["ok"]))
                return 3;
            tokens.push_back(uint64_t(r["token"].toString().getHexValue64()));
            controls.push_back(std::move(tcp));
            receivers.push_back(std::move(udp));
        }
        ll::Socket bad(ll::connectLocal());
        auto denied = request(bad.s, "{\"op\":\"pair\",\"pin\":\"wrong\",\"port\":5555}");
        if (bool(denied["ok"]))
            return 4;
        uint32_t zero = 0;
        if (!shared->producer.compare_exchange_strong(zero, GetCurrentProcessId())) {
            std::cerr << "Close DAW before probe\n";
            return 5;
        }
        ll::Block b;
        b.stream = 123;
        b.rate = 96000;
        for (unsigned i = 0; i < 256; ++i)
            b.samples[i] = float(std::sin(i * 0.1));
        shared->rate = 96000;
        for (int batch = 0; batch < 3; ++batch) {
            if (batch == 1)
                request(controls[0]->s, "{\"op\":\"disconnect\"}");
            if (batch == 2)
                request(controls[1]->s, "{\"op\":\"device\",\"mute\":true}");
            if (batch == 2)
                request(controls[2]->s, "{\"op\":\"device\",\"volume\":0.5,\"mono\":true}");
            for (int p = 0; p < 8; ++p) {
                b.timestamp = (batch * 8 + p) * 128;
                if (!shared->push(b))
                    return 6;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            for (int i = (batch ? 1 : 0); i < 5; ++i) {
                for (unsigned j = 0; j < 8 * (128 / packetFrames); ++j) {
                    std::array<uint8_t, ll::packetBytes> bytes;
                    int n = recv(receivers[i]->s, (char *)bytes.data(), int(bytes.size()), 0);
                    ll::Packet p;
                    if (n != int(ll::headerBytes + packetFrames * 8) ||
                        !ll::decode(std::span(bytes).first(size_t(n)), p) || p.token != tokens[i] ||
                        p.rate != 96000 ||
                        p.timestamp != uint64_t(batch * 8 * 128 + j * packetFrames))
                        return 7;
                    if (batch == 2 && i == 1) {
                        for (float x : p.samples)
                            if (x != 0)
                                return 8;
                    } else
                        for (unsigned k = 0; k < packetFrames * 2; ++k) {
                            auto index = (p.timestamp % 128) * 2 + k;
                            float expected =
                                batch == 2 && i == 2
                                    ? (b.samples[index & ~1u] + b.samples[(index & ~1u) + 1]) *
                                          0.25f
                                    : b.samples[index];
                            if (p.samples[k] != expected)
                                return 9;
                        }
                }
            }
        }
        shared->producer = 0;
        std::cout << "PASS actual shared-memory -> service -> UDP path; five "
                     "receivers; unauthorized pairing denied; disconnect "
                     "isolation; mute/volume/mono; 96 kHz PCM\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 10;
    }
}
