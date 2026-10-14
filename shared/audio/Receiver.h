#pragma once
#include "shared/protocol/AudioPacket.h"
#include <algorithm>
#include <atomic>
#include <chrono>
namespace ll {
class Receiver {
    static_assert(std::atomic<uint32_t>::is_always_lock_free &&
                  std::atomic<float>::is_always_lock_free);
    static constexpr uint32_t capacity = 32768;
    std::array<float, capacity * 2> audio{};
    alignas(64) std::atomic<uint32_t> write{0};
    alignas(64) std::atomic<uint32_t> read{0};
    std::atomic<uint32_t> rate{48000}, targetMs{20}, flush{0};
    std::array<Packet, 32> reorder{};
    std::array<bool, 32> present{};
    uint64_t token = 0;
    uint32_t expected = 0, stream = 0;
    bool started = false, primed = false;
    double fraction = 0;
    float envelope = 0;
    std::chrono::steady_clock::time_point arrival{};
    double jitterMs = 0;
    std::atomic<uint32_t> extraMs{0};
    bool append(const std::array<float, 256> &samples, unsigned frames) {
        auto w = write.load(std::memory_order_relaxed);
        auto r = read.load(std::memory_order_acquire);
        if (w - r + frames >= capacity) {
            overflow++;
            return false;
        }
        for (uint32_t i = 0; i < frames; ++i) {
            auto index = ((w + i) % capacity) * 2;
            audio[index] = samples[i * 2];
            audio[index + 1] = samples[i * 2 + 1];
        }
        write.store(w + frames, std::memory_order_release);
        return true;
    }

  public:
    void discardBufferedPlayback() noexcept { flush.store(1); }
    std::atomic<uint32_t> loss{0}, underruns{0}, overflow{0}, occupancy{0}, received{0};
    std::atomic<float> peak{0};
    void authorize(uint64_t value) {
        loss = 0;
        underruns = 0;
        overflow = 0;
        received = 0;
        occupancy = 0;
        peak = 0;
        arrival = {};
        jitterMs = 0;
        extraMs = 0;
        token = value;
        started = false;
        present.fill(false);
        flush = 1;
    }
    void mode(int m) { targetMs = m == 0 ? 8 : m == 2 ? 40 : 20; }
    uint32_t sampleRate() const { return rate.load(); }
    bool receive(std::span<const uint8_t> bytes) {
        Packet p;
        if (!decode(bytes, p) || p.token != token)
            return false;
        if (!started || p.stream != stream || rate.load() != p.rate) {
            present.fill(false);
            expected = p.sequence;
            stream = p.stream;
            rate = p.rate;
            started = true;
            flush = 1;
        }
        int32_t distance = int32_t(p.sequence - expected);
        if (distance < 0)
            return false;
        if (distance >= 32) {
            loss.fetch_add(uint32_t(distance));
            present.fill(false);
            expected = p.sequence;
            flush = 1;
        }
        auto now = std::chrono::steady_clock::now();
        if (arrival.time_since_epoch().count() != 0) {
            double interval = std::chrono::duration<double, std::milli>(now - arrival).count();
            double expectedMs = double(p.frames) * 1000 / p.rate;
            jitterMs += (std::abs(interval - expectedMs) - jitterMs) / 16;
            extraMs = uint32_t(std::clamp(jitterMs * 4, 0.0, 40.0));
        }
        arrival = now;
        auto index = p.sequence % 32;
        if (present[index])
            return false;
        reorder[index] = p;
        present[index] = true;
        received++;
        while (true) {
            auto i = expected % 32;
            if (present[i] && reorder[i].sequence == expected) {
                append(reorder[i].samples, reorder[i].frames);
                present[i] = false;
                ++expected;
            } else if (int32_t(p.sequence - expected) >= 3) {
                std::array<float, 256> silence{};
                append(silence, p.frames);
                loss++;
                expected++;
            } else
                break;
        }
        return true;
    }
    // Single audio consumer. No locks, allocations, network or OS calls.
    void render(float *left, float *right, uint32_t frames, double deviceRate) noexcept {
        auto r = read.load(std::memory_order_relaxed);
        auto w = write.load(std::memory_order_acquire);
        if (flush.exchange(0)) {
            r = w;
            fraction = 0;
            primed = false;
            envelope = 0;
        }
        auto sourceRate = rate.load();
        auto target =
            std::clamp(uint32_t(double(sourceRate) * (targetMs.load() + extraMs.load()) / 1000),
                       256u, capacity / 2);
        uint32_t available = w - r;
        float maximum = 0;
        if (!primed && available >= target)
            primed = true;
        double correction =
            std::clamp((double(available) - target) / double(target) * 0.0005, -0.002, 0.002);
        double ratio = double(sourceRate) / std::max(8000.0, deviceRate) * (1 + correction);
        bool underrun = false;
        for (uint32_t i = 0; i < frames; ++i) {
            if (primed && w - r >= uint32_t(ratio) + 2) {
                auto a = (r % capacity) * 2, b = ((r + 1) % capacity) * 2;
                envelope = std::min(1.f, envelope + 0.005f);
                left[i] = (audio[a] + float(fraction) * (audio[b] - audio[a])) * envelope;
                right[i] =
                    (audio[a + 1] + float(fraction) * (audio[b + 1] - audio[a + 1])) * envelope;
                fraction += ratio;
                auto consume = uint32_t(fraction);
                r += consume;
                fraction -= consume;
                maximum = std::max(maximum, std::max(std::abs(left[i]), std::abs(right[i])));
            } else {
                if (primed) {
                    underrun = true;
                    primed = false;
                    fraction = 0;
                    envelope = 0;
                }
                left[i] = right[i] = 0;
            }
        }
        if (underrun)
            underruns++;
        read.store(r, std::memory_order_release);
        occupancy = w - r;
        peak = maximum;
    }
};
} // namespace ll
