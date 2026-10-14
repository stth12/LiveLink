#include "shared/ipc/SharedMemory.h"
#include <iostream>
#include <juce_audio_utils/juce_audio_utils.h>
#include <thread>
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter();
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << "FAIL " << __LINE__ << ": " << #x << '\n';                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(int argc, char **argv) {
    juce::ScopedJuceInitialiser_GUI init;
    ll::Mapping mapping(true);
    auto *s = mapping.get();
    CHECK(s);
    CHECK(s->heartbeat == 0 || GetTickCount() - s->heartbeat.load() > 2000);
    s->read = s->write.load();
    s->producer = 0;
    s->heartbeat = GetTickCount();
    auto processor = std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
    std::this_thread::sleep_for(std::chrono::milliseconds(350));
    CHECK(s->producer == GetCurrentProcessId());
    juce::MidiBuffer midi;
    for (double rate : {44100., 48000., 88200., 96000.}) {
        s->heartbeat = GetTickCount();
        processor->prepareToPlay(rate, 511);
        uint64_t inputFrames = 0;
        for (int size : {1, 64, 128, 511}) {
            juce::AudioBuffer<float> b(2, size), original(2, size);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < size; ++i)
                    b.setSample(c, i, float(std::sin((inputFrames + i) * 0.04 + c) * 0.7));
            original.makeCopyOf(b);
            processor->processBlock(b, midi);
            for (int c = 0; c < 2; ++c)
                CHECK(memcmp(b.getReadPointer(c), original.getReadPointer(c),
                             size * sizeof(float)) == 0);
            inputFrames += size;
        }
        ll::Block block;
        unsigned packets = 0;
        while (s->pop(block)) {
            CHECK(block.rate == uint32_t(rate));
            CHECK(block.timestamp == uint64_t(packets) * 128);
            packets++;
        }
        CHECK(packets == inputFrames / 128);
        processor->releaseResources();
    }
    // An absent service must not stop or change the audio.
    s->heartbeat = 0;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    juce::AudioBuffer<float> b(2, 256);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 256; ++i)
            b.setSample(c, i, 0.25f);
    processor->processBlock(b, midi);
    CHECK(b.getSample(0, 255) == 0.25f);
    ll::Block block;
    CHECK(!s->pop(block));
    {
        auto editor = std::unique_ptr<juce::AudioProcessorEditor>(processor->createEditor());
        CHECK(editor);
        juce::Image image(juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true,
                          juce::SoftwareImageType());
        {
            juce::Graphics graphics(image);
            editor->paintEntireComponent(graphics, true);
        }
        CHECK(image.getPixelAt(0, 0).getARGB() == 0xff08080b);
        if (argc > 1) {
            juce::File file(argv[1]);
            auto output = file.createOutputStream();
            CHECK(output);
            output->setPosition(0);
            output->truncate();
            juce::PNGImageFormat png;
            CHECK(png.writeImageToStream(image, *output));
        }
    }
    processor.reset();
    CHECK(s->producer == 0);
    std::cout << "PASS actual JUCE processor: bit-identical pass-through, "
                 "shared-memory capture, 4 sample rates, irregular blocks, "
                 "offline-service safety and ownership cleanup\n";
}
