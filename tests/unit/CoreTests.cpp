#include "shared/ipc/SharedMemory.h"
#include <iostream>
#include <thread>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << "FAIL " << __LINE__ << ": " << #x << '\n';                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main() {
    ll::Block b;
    b.rate = 44100;
    b.stream = 77;
    b.timestamp = 0x123456789abcdef0;
    for (unsigned i = 0; i < 256; ++i)
        b.samples[i] = float(i) / 256;
    auto bytes = ll::encode(b, 0xfedcba9876543210, 99);
    ll::Packet p;
    CHECK(ll::decode(bytes, p));
    CHECK(p.rate == 44100 && p.stream == 77 && p.timestamp == b.timestamp &&
          p.token == 0xfedcba9876543210 && p.sequence == 99);
    CHECK(p.samples == b.samples);
    CHECK(bytes[0] == 'L' && bytes[3] == 'K');
    for (unsigned frames : {32u, 64u}) {
        auto shortPacket = ll::encode(b, 3, 7, 1, false, false, frames, frames);
        ll::Packet shortDecoded;
        CHECK(ll::decode(std::span(shortPacket).first(ll::headerBytes + frames * 8), shortDecoded));
        CHECK(shortDecoded.frames == frames);
        CHECK(shortDecoded.timestamp == b.timestamp + frames);
        CHECK(shortDecoded.samples[0] == b.samples[frames * 2]);
        CHECK(!ll::decode(shortPacket, shortDecoded));
    }
    CHECK(!ll::decode(std::span(bytes).first(60), p));
    auto corrupt = bytes;
    corrupt[7] = 2;
    CHECK(!ll::decode(corrupt, p));
    bytes = ll::encode(b, 1, 1, 0.5, true);
    CHECK(ll::decode(bytes, p));
    CHECK(p.samples[0] == p.samples[1] && p.samples[0] == (b.samples[0] + b.samples[1]) * 0.25f);
    bytes = ll::encode(b, 1, 1, 1, false, true);
    CHECK(ll::decode(bytes, p));
    for (float x : p.samples)
        CHECK(x == 0);
    auto ring = std::make_unique<ll::Shared>();
    for (unsigned i = 0; i < ll::slots; ++i) {
        b.timestamp = i;
        CHECK(ring->push(b));
    }
    CHECK(!ring->push(b));
    CHECK(ring->drops == 1);
    for (unsigned i = 0; i < ll::slots; ++i) {
        CHECK(ring->pop(b));
        CHECK(b.timestamp == i);
    }
    CHECK(!ring->pop(b));
    std::atomic<bool> failed = false;
    constexpr uint64_t count = 100000;
    std::jthread producer([&] {
        ll::Block x;
        for (uint64_t i = 0; i < count; ++i) {
            x.timestamp = i;
            while (!ring->push(x))
                std::this_thread::yield();
        }
    });
    for (uint64_t i = 0; i < count; ++i) {
        while (!ring->pop(b))
            std::this_thread::yield();
        CHECK(b.timestamp == i);
    }
    producer.join();
    ring->write = UINT32_MAX - 10;
    ring->read = UINT32_MAX - 10;
    for (unsigned i = 0; i < 100; ++i) {
        b.timestamp = i;
        CHECK(ring->push(b));
        CHECK(ring->pop(b));
        CHECK(b.timestamp == i);
    }
    std::cout << "PASS protocol, malformed packets, gain, mono, mute, ring "
                 "capacity, 100000 concurrent blocks, index wrap\n";
}
