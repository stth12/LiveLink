#include "shared/transport/Sockets.h"
#include "shared/ipc/SharedMemory.h"
#include <juce_core/juce_core.h>
#include <thread>
#include <iostream>
static juce::var command(SOCKET socket, const juce::String &json) {
    ll::sendText(socket, json.toStdString() + "\n");
    return juce::JSON::parse(ll::readLine(socket));
}
int main() {
    ll::Network network;
    ll::Mapping mapping(false);
    auto *s = mapping.get();
    if (!s)
        return 1;
    std::vector<std::unique_ptr<ll::Socket>> control, receivers;
    std::array<uint64_t, 5> tokens{};
    for (int i = 0; i < 5; ++i) {
        auto u = std::make_unique<ll::Socket>(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(u->s, (sockaddr *)&address, sizeof(address)))
            return 2;
        int size = sizeof(address);
        getsockname(u->s, (sockaddr *)&address, &size);
        DWORD timeout = 500;
        setsockopt(u->s, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
        int buffer = 2 * 1024 * 1024;
        setsockopt(u->s, SOL_SOCKET, SO_RCVBUF, (char *)&buffer, sizeof(buffer));
        auto c = std::make_unique<ll::Socket>(ll::connectLocal());
        auto response =
            command(c->s, "{\"op\":\"pair\",\"pin\":\"" + juce::String(s->pin.load()) +
                              "\",\"name\":\"TEST / Stress " + juce::String(i) +
                              "\",\"port\":" + juce::String(ntohs(address.sin_port)) + "}");
        if (!bool(response["ok"]))
            return 3;
        tokens[i] = uint64_t(response["token"].toString().getHexValue64());
        control.push_back(std::move(c));
        receivers.push_back(std::move(u));
    }
    uint32_t zero = 0;
    if (!s->producer.compare_exchange_strong(zero, uint32_t(GetCurrentProcessId())))
        return 4;
    std::atomic<bool> finished = false, failed = false;
    std::array<std::atomic<uint32_t>, 5> counts{}, loss{};
    std::vector<std::jthread> readers;
    // Receiver 0 deliberately never consumes UDP: its socket fills while four others continue.
    for (int i = 1; i < 5; ++i)
        readers.emplace_back([&, i] {
            uint32_t previous = 0;
            bool seen = false;
            while (!finished) {
                std::array<uint8_t, ll::packetBytes> bytes{};
                int size = recv(receivers[i]->s, (char *)bytes.data(), int(bytes.size()), 0);
                if (size < 0)
                    continue;
                ll::Packet p;
                if (!ll::decode(std::span(bytes).first(size_t(size)), p) || p.token != tokens[i] ||
                    p.stream != 987) {
                    failed = true;
                    continue;
                }
                if (seen && p.sequence != previous + 1)
                    loss[i] += p.sequence - previous - 1;
                previous = p.sequence;
                seen = true;
                counts[i]++;
            }
        });
    std::jthread pings([&] {
        while (!finished) {
            for (auto &c : control)
                command(c->s, "{\"op\":\"ping\"}");
            for (int i = 0; i < 10 && !finished; ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
    ll::Block b;
    b.rate = 48000;
    b.stream = 987;
    for (unsigned i = 0; i < 256; ++i)
        b.samples[i] = float(std::sin(i * 0.1) * 0.25);
    s->rate = 48000;
    s->peakL = std::bit_cast<uint32_t>(0.25f);
    s->peakR = std::bit_cast<uint32_t>(0.25f);
    auto begin = std::chrono::steady_clock::now();
    constexpr unsigned packets = 3750;
    unsigned dropped = 0;
    for (unsigned i = 0; i < packets; ++i) {
        b.timestamp = uint64_t(i) * 128;
        if (!s->push(b))
            dropped++;
        std::this_thread::sleep_until(
            begin + std::chrono::microseconds(uint64_t(i + 1) * 128000000 / 48000));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    finished = true;
    readers.clear();
    pings.join();
    s->producer = 0;
    for (int i = 1; i < 5; ++i) {
        std::cout << "receiver " << i << ": " << counts[i] << " packets, " << loss[i] << " gaps\n";
        if (counts[i] < packets - 2 || loss[i] != 0)
            failed = true;
    }
    std::cout << "10-second loopback stress: one stalled UDP consumer, four active consumers; "
                 "capture drops "
              << dropped << "\n";
    return failed || dropped ? 5 : 0;
}
