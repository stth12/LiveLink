#include "shared/ipc/SharedMemory.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <thread>
class Processor;
class Editor : public juce::AudioProcessorEditor, private juce::Timer {
    Processor &p;
    juce::TextButton open{"OPEN LIVE LINK"};

  public:
    explicit Editor(Processor &);
    void paint(juce::Graphics &) override;
    void resized() override;
    void timerCallback() override { repaint(); }
};
class Processor : public juce::AudioProcessor {
    std::unique_ptr<ll::Mapping> mapping;
    std::atomic<ll::Shared *> shared{nullptr};
    std::jthread worker;
    ll::Block partial;
    unsigned offset = 0;
    uint64_t clock = 0;
    uint32_t stream = 0;
    bool capturing = false;
    std::atomic<uint32_t> sampleRate{0};
    std::atomic<bool> available{false};

  public:
    std::atomic<float> left{0}, right{0};
    std::atomic<bool> owned{false};
    Processor()
        : AudioProcessor(BusesProperties()
                             .withInput("Input", juce::AudioChannelSet::stereo(), true)
                             .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
        worker = std::jthread([this](std::stop_token stop) {
            while (!stop.stop_requested()) {
                if (!mapping) {
                    try {
                        auto m = std::make_unique<ll::Mapping>(false);
                        if (m->get()) {
                            mapping = std::move(m);
                            shared.store(mapping->get(), std::memory_order_release);
                        }
                    } catch (...) {
                    }
                }
                auto *s = shared.load();
                bool online =
                    s && s->heartbeat.load() != 0 && GetTickCount() - s->heartbeat.load() < 2000;
                if (online && !owned) {
                    uint32_t zero = 0;
                    owned =
                        s->producer.compare_exchange_strong(zero, uint32_t(GetCurrentProcessId()));
                }
                available = online && owned.load();
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        });
    }
    ~Processor() override {
        worker.request_stop();
        worker.join();
        if (auto *s = shared.load()) {
            uint32_t pid = GetCurrentProcessId();
            if (owned)
                s->producer.compare_exchange_strong(pid, 0);
        }
    }
    void prepareToPlay(double rate, int) override {
        sampleRate = uint32_t(rate);
        offset = 0;
        clock = 0;
        stream = uint32_t(juce::Random::getSystemRandom().nextInt());
    }
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout &l) const override {
        return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
               l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }
    void processBlock(juce::AudioBuffer<float> &b, juce::MidiBuffer &) override {
        juce::ScopedNoDenormals noDenormals;
        float peaks[2]{};
        auto *s = shared.load(std::memory_order_acquire);
        bool capture = s && available.load();
        if (capture != capturing) {
            offset = 0;
            ++stream;
            capturing = capture;
        }
        for (int n = 0; n < b.getNumSamples(); ++n) {
            for (int c = 0; c < 2; ++c) {
                float x = b.getReadPointer(c)[n];
                peaks[c] = std::max(peaks[c], std::abs(x));
                if (capture)
                    partial.samples[offset * 2 + c] = x;
            }
            if (capture) {
                if (offset == 0) {
                    partial.timestamp = clock;
                    partial.rate = sampleRate;
                    partial.stream = stream;
                }
                if (++offset == 128) {
                    s->push(partial);
                    offset = 0;
                }
            }
            ++clock;
        }
        left = peaks[0];
        right = peaks[1];
        if (capture) {
            s->rate = sampleRate.load();
            s->peakL = std::bit_cast<uint32_t>(peaks[0]);
            s->peakR = std::bit_cast<uint32_t>(peaks[1]);
        }
    }
    ll::Shared *status() const { return shared.load(); }
    const juce::String getName() const override { return "Live Link"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor *createEditor() override { return new Editor(*this); }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String &) override {}
    void getStateInformation(juce::MemoryBlock &dest) override { dest.setSize(0); }
    void setStateInformation(const void *, int) override {}
};
Editor::Editor(Processor &x) : AudioProcessorEditor(x), p(x) {
    setResizable(true, true);
    setResizeLimits(360, 280, 720, 540);
    setSize(440, 360);
    addAndMakeVisible(open);
    open.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffff2d8d));
    open.onClick = [] {
        auto file = juce::File::getSpecialLocation(juce::File::globalApplicationsDirectory)
                        .getChildFile("Live Link/Live Link.exe");
        if (!file.existsAsFile())
            file = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                       .getSiblingFile("Live Link.exe");
        if (!file.startAsProcess())
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon, "Live Link",
                "Install Live Link Desktop, or launch it from your build folder.");
    };
    startTimerHz(30);
}
void Editor::resized() { open.setBounds(24, getHeight() - 54, getWidth() - 48, 32); }
void Editor::paint(juce::Graphics &g) {
    g.fillAll(juce::Colour(0xff08080b));
    g.setColour(juce::Colour(0xfff5f5f7));
    g.setFont(28.f);
    g.drawText("LIVE LINK", 24, 20, getWidth() - 48, 40, juce::Justification::left);
    auto *s = p.status();
    bool online = s && s->heartbeat.load() != 0 && GetTickCount() - s->heartbeat.load() < 2000;
    g.setFont(14.f);
    g.setColour(juce::Colour(online ? 0xff40d99b : 0xffffbd59));
    g.drawText(online ? (p.owned ? "SERVICE ONLINE" : "READY / OTHER PLUGIN OWNS CAPTURE")
                      : "SERVICE OFFLINE",
               24, 68, getWidth() - 48, 24, juce::Justification::left);
    g.setColour(juce::Colour(0xff92929d));
    g.drawText(online
                   ? juce::String(s->receivers.load()) + " receivers  /  " +
                         juce::String(getAudioProcessor()->getSampleRate(), 0) + " Hz / stereo PCM"
                   : "Launch Live Link Desktop to begin streaming",
               24, 98, getWidth() - 48, 22, juce::Justification::left);
    for (int c = 0; c < 2; ++c) {
        float peak = c ? p.right.load() : p.left.load();
        g.setColour(juce::Colour(0xff19191f));
        g.fillRoundedRectangle(24.f, 140.f + c * 20, float(getWidth() - 48), 10, 5);
        g.setColour(juce::Colour(0xffff2d8d));
        g.fillRoundedRectangle(24.f, 140.f + c * 20,
                               float(getWidth() - 48) * std::clamp(peak, 0.f, 1.f), 10, 5);
    }
    if (online) {
        std::array<char, 401> names{};
        auto revision = s->namesRevision.load();
        if (!(revision & 1)) {
            for (unsigned i = 0; i < 100; ++i) {
                auto chunk = s->receiverNames[i].load();
                memcpy(names.data() + i * 4, &chunk, 4);
            }
            if (revision == s->namesRevision.load()) {
                g.setColour(juce::Colour(0xff92929d));
                g.setFont(12.f);
                int rows = std::clamp((getHeight() - 260) / 18, 0, 5);
                for (int i = 0; i < rows; ++i)
                    g.drawText(juce::String::fromUTF8(names.data() + i * 80, 79), 24, 190 + i * 18,
                               getWidth() - 48, 18, juce::Justification::left);
            }
        }
    }
}
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new Processor; }
