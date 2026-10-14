#pragma once
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>
namespace ll {
constexpr unsigned framesPerPacket = 128, channels = 2, headerBytes = 48;
constexpr unsigned packetBytes = headerBytes + framesPerPacket * channels * 4;
inline void put32(uint8_t *p, uint32_t n) {
    for (int i = 0; i < 4; ++i)
        p[i] = uint8_t(n >> (24 - i * 8));
}
inline uint32_t get32(const uint8_t *p) {
    return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}
inline void put64(uint8_t *p, uint64_t n) {
    put32(p, uint32_t(n >> 32));
    put32(p + 4, uint32_t(n));
}
inline uint64_t get64(const uint8_t *p) { return uint64_t(get32(p)) << 32 | get32(p + 4); }
struct Block {
    uint64_t timestamp{};
    uint32_t rate{48000};
    uint32_t stream{};
    std::array<float, 256> samples{};
};
struct Packet {
    uint64_t token{}, timestamp{};
    uint32_t stream{}, sequence{}, rate{}, frames{128};
    std::array<float, 256> samples{};
};
inline std::array<uint8_t, packetBytes> encode(const Block &b, uint64_t token, uint32_t seq,
                                               float gain = 1, bool mono = false, bool mute = false,
                                               unsigned frames = 128, unsigned offset = 0) {
    if ((frames != 32 && frames != 64 && frames != 128) || offset + frames > 128)
        throw std::invalid_argument("Invalid packet frame range");
    std::array<uint8_t, packetBytes> p{};
    put32(p.data(), 0x4c4c4e4b);
    put32(p.data() + 4, 1);
    put64(p.data() + 8, token);
    put32(p.data() + 16, b.stream);
    put32(p.data() + 20, seq);
    put64(p.data() + 24, b.timestamp + offset);
    put32(p.data() + 32, b.rate);
    put32(p.data() + 36, frames);
    put32(p.data() + 40, 2);
    put32(p.data() + 44, 0);
    for (unsigned i = 0; i < frames * 2; ++i) {
        unsigned source = i + offset * 2;
        float x = mono ? (b.samples[source & ~1u] + b.samples[(source & ~1u) + 1]) * 0.5f
                       : b.samples[source];
        x = mute ? 0 : x * gain;
        if (!std::isfinite(x))
            x = 0;
        put32(p.data() + 48 + i * 4, std::bit_cast<uint32_t>(x));
    }
    return p;
}
inline bool decode(std::span<const uint8_t> p, Packet &b) {
    if (p.size() < headerBytes || p.size() > packetBytes || get32(p.data()) != 0x4c4c4e4b ||
        get32(p.data() + 4) != 1 || get32(p.data() + 40) != 2 || get32(p.data() + 44) != 0)
        return false;
    b.frames = get32(p.data() + 36);
    if ((b.frames != 32 && b.frames != 64 && b.frames != 128) ||
        p.size() != headerBytes + b.frames * 8)
        return false;
    b.token = get64(p.data() + 8);
    b.stream = get32(p.data() + 16);
    b.sequence = get32(p.data() + 20);
    b.timestamp = get64(p.data() + 24);
    b.rate = get32(p.data() + 32);
    if (b.rate < 8000 || b.rate > 192000)
        return false;
    b.samples.fill(0);
    for (unsigned i = 0; i < b.frames * 2; ++i) {
        b.samples[i] = std::bit_cast<float>(get32(p.data() + 48 + i * 4));
        if (!std::isfinite(b.samples[i]))
            return false;
    }
    return true;
}
} // namespace ll
