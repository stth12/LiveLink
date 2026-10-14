#include <iostream>
#include <juce_audio_utils/juce_audio_utils.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << "FAIL " << __LINE__ << ": " << #x << '\n';                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(int argc, char **argv) {
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2)
        return 2;
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile(descriptions, argv[1]);
    CHECK(descriptions.size() == 1);
    juce::String error;
    auto plugin = format.createInstanceFromDescription(*descriptions[0], 48000, 256, error);
    if (!plugin) {
        std::cerr << error << '\n';
        return 3;
    }
    juce::MidiBuffer midi;
    for (double rate : {44100., 48000., 88200., 96000.}) {
        plugin->setPlayConfigDetails(2, 2, rate, 256);
        plugin->prepareToPlay(rate, 256);
        juce::AudioBuffer<float> buffer(2, 256), original(2, 256);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 256; ++i)
                buffer.setSample(c, i, float(std::sin(i * 0.13 + c) * 0.2));
        original.makeCopyOf(buffer);
        plugin->processBlock(buffer, midi);
        for (int c = 0; c < 2; ++c)
            CHECK(memcmp(buffer.getReadPointer(c), original.getReadPointer(c),
                         256 * sizeof(float)) == 0);
        plugin->releaseResources();
    }
    auto *editor = plugin->createEditorIfNeeded();
    CHECK(editor);
    editor->setVisible(true);
    delete editor;
    std::cout << "PASS compiled VST3 bundle loads in JUCE VST3 host; four sample rates "
                 "bit-identical; editor created. Named DAWs remain unverified.\n";
}
