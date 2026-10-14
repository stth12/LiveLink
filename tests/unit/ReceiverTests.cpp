#include "shared/audio/Receiver.h"
#include <chrono>
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
    auto r = std::make_unique<ll::Receiver>();
    r->authorize(42);
    ll::Block b;
    b.stream = 1;
    float l[512], right[512];
    uint32_t sequence = 0;
    for (auto sourceRate : {44100u, 48000u, 88200u, 96000u}) {
        b.stream++;
        b.rate = sourceRate;
        for (float &x : b.samples)
            x = 0.25f;
        auto packet = ll::encode(b, 99, sequence++);
        CHECK(!r->receive(packet));
        packet = ll::encode(b, 42, sequence++);
        CHECK(r->receive(packet));
        r->render(l, right, 1, 48000);
        for (unsigned i = 0; i < 64; ++i) {
            b.timestamp += 128;
            CHECK(r->receive(ll::encode(b, 42, sequence++)));
        }
        r->render(l, right, 512, 48000);
        CHECK(l[511] > 0.24f && l[511] < 0.26f);
        CHECK(right[511] == l[511]);
        CHECK(r->sampleRate() == sourceRate);
        CHECK(r->occupancy > 0);
    }
    // Reordering waits for a bounded gap; a missing packet is concealed after 3
    // successors.
    b.stream++;
    r->receive(ll::encode(b, 42, 1000));
    r->render(l, right, 1, 48000);
    auto oldLoss = r->loss.load();
    r->receive(ll::encode(b, 42, 1002));
    r->receive(ll::encode(b, 42, 1001));
    CHECK(r->loss == oldLoss);
    r->receive(ll::encode(b, 42, 1006));
    CHECK(r->loss > oldLoss);
    CHECK(!r->receive(ll::encode(b, 42, 1001)));
    for (uint32_t i = 1007; i < 1071; ++i)
        r->receive(ll::encode(b, 42, i));
    for (int i = 0; i < 100; ++i)
        r->render(l, right, 512, 48000);
    CHECK(r->underruns > 0);
    for (float x : l)
        CHECK(x == 0);
    // Timed concurrent producer/consumer exercises the same engine shipped on
    // iOS.
    auto stress = std::make_unique<ll::Receiver>();
    stress->authorize(7);
    std::atomic<bool> done = false;
    std::jthread producer([&] {
        ll::Block x;
        x.stream = 55;
        x.rate = 48000;
        x.samples.fill(0.1f);
        for (uint32_t i = 0; i < 5000; ++i) {
            x.timestamp = uint64_t(i) * 128;
            stress->receive(ll::encode(x, 7, i));
            if (i % 4 == 0)
                std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        done = true;
    });
    unsigned renders = 0;
    auto begin = std::chrono::steady_clock::now();
    while (!done) {
        stress->render(l, right, 128, 48000);
        for (unsigned i = 0; i < 128; ++i) {
            CHECK(std::isfinite(l[i]) && std::abs(l[i]) <= 1);
            CHECK(l[i] == right[i]);
        }
        renders++;
    }
    producer.join();
    auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    std::cout << "PASS receiver token validation, 4 source rates, resampling, "
                 "reorder, loss concealment, underrun silence; "
              << renders << " concurrent render calls in " << elapsed
              << " s (not acoustic latency)\n";
}
