#include "shared/ipc/SharedMemory.h"
#include "shared/transport/Sockets.h"
#include <deque>
#include <juce_gui_basics/juce_gui_basics.h>
#include <mutex>
#include <thread>
using juce::var;
static var obj() { return var(new juce::DynamicObject); }
static void prop(var &v, const char *k, var x) { v.getDynamicObject()->setProperty(k, x); }
static std::string stringify(const var &v) {
    return juce::JSON::toString(v, true).toStdString() + "\n";
}
static bool launchService() {
    auto exe = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                   .getSiblingFile("Live Link Service.exe");
    if (!exe.existsAsFile())
        return false;
    std::wstring command = L"\"";
    command += exe.getFullPathName().toWideCharPointer();
    command += L"\"";
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        nullptr, &si, &pi))
        return false;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}
class Control {
    ll::Network network;
    std::mutex mutex;
    var current;
    std::deque<var> commands;
    std::jthread worker;

  public:
    Control() {
        worker = std::jthread([this](std::stop_token stop) {
            while (!stop.stop_requested()) {
                var status;
                try {
                    ll::Mapping map(false);
                    if (auto *s = map.get()) {
                        ll::Socket socket(ll::connectLocal());
                        if (socket.s != INVALID_SOCKET) {
                            auto auth = obj();
                            prop(auth, "op", "admin");
                            prop(auth, "token",
                                 juce::String::toHexString((juce::int64)s->adminToken.load()));
                            ll::sendText(socket.s, stringify(auth));
                            status = juce::JSON::parse(ll::readLine(socket.s));
                            std::deque<var> todo;
                            {
                                std::lock_guard lock(mutex);
                                todo.swap(commands);
                            }
                            for (auto &c : todo) {
                                if (!ll::sendText(socket.s, stringify(c)))
                                    break;
                                auto reply = juce::JSON::parse(ll::readLine(socket.s));
                                if (reply.hasProperty("devices"))
                                    status = reply;
                            }
                        }
                    }
                } catch (const std::exception &e) {
                    status = obj();
                    prop(status, "error", e.what());
                }
                {
                    std::lock_guard lock(mutex);
                    current = status;
                }
                for (int i = 0; i < 5 && !stop.stop_requested(); ++i)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }
    ~Control() {
        worker.request_stop();
        worker.join();
    }
    var state() {
        std::lock_guard lock(mutex);
        return current;
    }
    void send(var v) {
        std::lock_guard lock(mutex);
        if (commands.size() < 64)
            commands.push_back(v);
    }
    void command(const char *op, const char *key = nullptr, var value = {}) {
        auto v = obj();
        prop(v, "op", op);
        if (key)
            prop(v, key, value);
        send(v);
    }
};
#include "desktop/include/SettingsPanel.h"
class DeviceCard : public juce::Component {
    Control &control;
    juce::String id;
    var data;
    juce::Slider volume;
    juce::ToggleButton mute{"Mute"}, mono{"Mono"};
    juce::TextButton connection;
    juce::ComboBox mode;
    void change(const char *k, var x) {
        auto v = obj();
        prop(v, "op", "device");
        prop(v, "id", id);
        prop(v, k, x);
        control.send(v);
    }

  public:
    DeviceCard(Control &c, juce::String key) : control(c), id(key) {
        for (auto *item :
             std::initializer_list<juce::Component *>{&volume, &mute, &mono, &connection, &mode})
            addAndMakeVisible(item);
        volume.setRange(0, 1, 0.01);
        volume.setSliderStyle(juce::Slider::LinearHorizontal);
        volume.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        volume.onValueChange = [this] { change("volume", volume.getValue()); };
        mute.onClick = [this] { change("mute", mute.getToggleState()); };
        mono.onClick = [this] { change("mono", mono.getToggleState()); };
        mode.addItemList({"Ultra low", "Balanced", "Stable"}, 1);
        mode.onChange = [this] { change("mode", mode.getSelectedId() - 1); };
        connection.onClick = [this] { change("connected", !bool(data["connected"])); };
    }
    void update(var d) {
        data = d;
        if (!volume.isMouseButtonDown())
            volume.setValue(double(d["volume"]), juce::dontSendNotification);
        mute.setToggleState(bool(d["mute"]), juce::dontSendNotification);
        mono.setToggleState(bool(d["mono"]), juce::dontSendNotification);
        mode.setSelectedId(int(d["mode"]) + 1, juce::dontSendNotification);
        connection.setButtonText(bool(d["connected"]) ? "Disconnect" : "Connect");
        repaint();
    }
    void resized() override {
        auto r = getLocalBounds().reduced(18);
        auto bottom = r.removeFromBottom(32);
        connection.setBounds(bottom.removeFromRight(112));
        mono.setBounds(bottom.removeFromRight(70));
        mute.setBounds(bottom.removeFromRight(70));
        mode.setBounds(bottom.removeFromRight(140).reduced(4, 0));
        volume.setBounds(bottom);
    }
    void paint(juce::Graphics &g) override {
        g.setColour(juce::Colour(0xff111116));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 12);
        g.setColour(juce::Colour(0xff292932));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 12, 1);
        g.setFont(18.f);
        g.setColour(juce::Colour(0xfff5f5f7));
        g.drawText(data["name"].toString(), 18, 12, getWidth() - 36, 25, juce::Justification::left);
        g.setFont(12.f);
        g.setColour(juce::Colour(0xff92929d));
        auto text = juce::String(bool(data["connected"])
                                     ? (bool(data["streaming"]) ? "STREAMING" : "CONNECTED")
                                     : "PAUSED") +
                    " / Wi-Fi   |   " + data["packets"].toString() + " packets   |   " +
                    (bool(data["hasTelemetry"])
                         ? juce::String("loss ") + data["loss"].toString() + " / underruns " +
                               data["underruns"].toString() + " / buffer estimate " +
                               juce::String(double(data["bufferEstimateMs"]), 1) + " ms"
                         : juce::String("Receiver diagnostics pending"));
        g.drawText(text, 18, 43, getWidth() - 36, 20, juce::Justification::left);
        g.setColour(juce::Colour(0xff292932));
        g.fillRoundedRectangle(float(getWidth() - 150), 22, 120, 6, 3);
        g.setColour(juce::Colour(0xffff2d8d));
        g.fillRoundedRectangle(float(getWidth() - 150), 22,
                               120 * std::clamp(float(data["level"]), 0.f, 1.f), 6, 3);
    }
};
class Dashboard : public juce::Component, private juce::Timer {
    Control control;
    var status;
    juce::TextButton settingsButton{"SETTINGS"};
    juce::LookAndFeel_V4 theme;
    juce::TextButton start{"START SERVICE"}, all{"CONNECT ALL"}, none{"DISCONNECT ALL"},
        rotate{"NEW PAIRING PIN"}, logs{"SAVE DIAGNOSTICS"};
    juce::Viewport viewport;
    juce::Component content;
    std::vector<std::unique_ptr<DeviceCard>> cards;
    juce::StringArray keys;
    juce::Label info;

  public:
    Dashboard() {
        theme.setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff08080b));
        theme.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff8b1e59));
        theme.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff19191f));
        theme.setColour(juce::Slider::thumbColourId, juce::Colour(0xffff2d8d));
        theme.setColour(juce::Slider::trackColourId, juce::Colour(0xffff2d8d));
        theme.setColour(juce::ScrollBar::thumbColourId,juce::Colour(0xffff2d8d));setLookAndFeel(&theme);
        for (auto *c : std::initializer_list<juce::Component *>{&start, &all, &none, &rotate, &logs,
                                                                &viewport, &info})
            addAndMakeVisible(c);
        viewport.setViewedComponent(&content, false);
        info.setText("Wi-Fi / independent playback     USB unavailable / awaiting "
                     "hardware validation",
                     juce::dontSendNotification);
        info.setColour(juce::Label::textColourId, juce::Colour(0xff92929d));
        start.onClick = [] {
            if (!launchService())
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon, "Live Link",
                    "Background process executable is missing or could not start.");
        };
        all.onClick = [this] { control.command("all", "connected", true); };
        none.onClick = [this] { control.command("all", "connected", false); };
        rotate.onClick = [this] { control.command("rotatePin"); };
        logs.onClick = [this] {
            auto folder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                              .getChildFile("Live Link Diagnostics");
            folder.createDirectory();
            auto file = folder.getChildFile(
                "status-" + juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S") + ".json");
            auto safe = status;
            if (auto *o = safe.getDynamicObject())
                o->removeProperty("pin");
            file.replaceWithText(juce::JSON::toString(safe));
            file.revealToUser();
        };
        addAndMakeVisible(settingsButton);
        settingsButton.onClick = [this] {
            juce::DialogWindow::LaunchOptions options;
            options.content.setOwned(new SettingsPanel(control));
            options.dialogTitle = "Live Link Settings";
            options.dialogBackgroundColour = juce::Colour(0xff111116);
            options.useNativeTitleBar = true;
            options.resizable = false;
            options.launchAsync();
        };
        setSize(960, 740);
        startTimerHz(10);
        launchService();
    }
    ~Dashboard() override {
        stopTimer();
        setLookAndFeel(nullptr);
    }
    void timerCallback() override {
        status = control.state();
        juce::StringArray newKeys;
        if (auto *a = status["devices"].getArray())
            for (auto &d : *a)
                newKeys.add(d["id"].toString());
        if (newKeys != keys) {
            cards.clear();
            keys = newKeys;
            for (auto &k : keys) {
                auto card = std::make_unique<DeviceCard>(control, k);
                content.addAndMakeVisible(*card);
                cards.push_back(std::move(card));
            }
            resized();
        }
        if (auto *a = status["devices"].getArray())
            for (int i = 0; i < a->size() && i < int(cards.size()); ++i)
                cards[i]->update((*a)[i]);
        bool online = bool(status["ok"]);
        for (auto *b : {&all, &none, &rotate, &logs})
            b->setEnabled(online);
        start.setEnabled(!online);
        repaint();
    }
    void resized() override {
        settingsButton.setBounds(getWidth() - 160, 36, 130, 32);
        auto r = getLocalBounds().reduced(30);
        r.removeFromTop(260);
        auto buttons = r.removeFromTop(34);
        start.setBounds(buttons.removeFromLeft(145).reduced(3, 0));
        all.setBounds(buttons.removeFromLeft(130).reduced(3, 0));
        none.setBounds(buttons.removeFromLeft(155).reduced(3, 0));
        rotate.setBounds(buttons.removeFromLeft(160).reduced(3, 0));
        logs.setBounds(buttons.removeFromLeft(160).reduced(3, 0));
        r.removeFromTop(20);
        info.setBounds(r.removeFromBottom(30));
        viewport.setBounds(r);
        content.setSize(std::max(100, r.getWidth() - 16),
                        std::max(r.getHeight(), int(cards.size()) * 134));
        for (size_t i = 0; i < cards.size(); ++i)
            cards[i]->setBounds(0, int(i) * 134, content.getWidth(), 120);
    }
    void paint(juce::Graphics &g) override {
        g.fillAll(juce::Colour(0xff08080b));
        g.setColour(juce::Colour(0xfff5f5f7));
        g.setFont(32.f);
        g.drawText("LIVE LINK", 32, 25, 400, 45, juce::Justification::left);
        g.setFont(13.f);
        g.setColour(juce::Colour(0xff92929d));
        g.drawText("Your Sound. Everywhere.", 32, 74, 400, 20, juce::Justification::left);
        g.setColour(juce::Colour(0xff111116));
        g.fillRoundedRectangle(30, 115, float(getWidth() - 60), 145, 14);
        bool online = bool(status["ok"]);
        bool streaming = bool(status["streaming"]) && int(status["activeReceivers"]) > 0;
        g.setColour(juce::Colour(online ? 0xff40d99b : 0xffffbd59));
        g.setFont(23.f);
        g.drawText(online ? (streaming ? "STREAMING" : "READY TO CONNECT") : "SERVICE OFFLINE", 50,
                   132, getWidth() - 100, 32, juce::Justification::left);
        g.setFont(14.f);
        g.setColour(juce::Colour(0xfff5f5f7));
        g.drawText("Pairing PIN: " + (online ? status["pin"].toString() : juce::String("/")) +
                       "     " +
                       (int(status["sampleRate"]) > 0 ? status["sampleRate"].toString() + " Hz"
                                                      : juce::String("No audio source")) +
                       "     " + juce::String(cards.size()) + " paired receivers",
                   50, 175, getWidth() - 100, 24, juce::Justification::left);
        g.setColour(juce::Colour(0xff92929d));
        g.drawText((int(status["mdnsStatus"]) == 0
                        ? juce::String("Network discovery ready")
                        : juce::String("Discovery unavailable / connect by address")) +
                       "   /   Capture drops: " + status["captureDrops"].toString(),
                   50, 208, getWidth() - 100, 20, juce::Justification::left);
        for (int c = 0; c < 2; ++c) {
            float peak = online && streaming ? float(status[c ? "peakR" : "peakL"]) : 0;
            g.setColour(juce::Colour(0xff292932));
            g.fillRect(getWidth() - 220, 140 + c * 17, 140, 8);
            g.setColour(juce::Colour(0xffff2d8d));
            g.fillRect(getWidth() - 220, 140 + c * 17, int(140 * std::clamp(peak, 0.f, 1.f)), 8);
        }
        if (cards.empty()) {
            g.setColour(juce::Colour(0xff92929d));
            g.setFont(18.f);
            g.drawText("Open Live Link on your iPhone and enter the pairing PIN.", 50, 365,
                       getWidth() - 100, 40, juce::Justification::centred);
        }
    }
};
class App : public juce::JUCEApplication {
    class Window : public juce::DocumentWindow {
      public:
        Window() : DocumentWindow("Live Link", juce::Colour(0xff08080b), allButtons) {
            setUsingNativeTitleBar(true);
            setContentOwned(new Dashboard, true);
            setResizable(true, true);
            setResizeLimits(860, 540, 1800, 1400);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }
        void closeButtonPressed() override {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };
    std::unique_ptr<Window> window;

  public:
    const juce::String getApplicationName() override { return "Live Link"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    void initialise(const juce::String &args) override {
        window = std::make_unique<Window>();
        auto tokens = juce::StringArray::fromTokens(args, true);
        if (tokens.size() == 2 && tokens[0] == "--snapshot") {
            window->setVisible(false);
            auto path = tokens[1].unquoted();
            juce::Timer::callAfterDelay(1500, [this, path] {
                auto *c = window->getContentComponent();
                juce::Image image(juce::Image::ARGB, c->getWidth(), c->getHeight(), true,
                                  juce::SoftwareImageType());
                {
                    juce::Graphics graphics(image);
                    c->paintEntireComponent(graphics, true);
                }
                juce::File file(path);
                if (auto output = file.createOutputStream()) {
                    output->setPosition(0);
                    output->truncate();
                    juce::PNGImageFormat png;
                    png.writeImageToStream(image, *output);
                }
                quit();
            });
        }
    }
    void shutdown() override { window.reset(); }
};
START_JUCE_APPLICATION(App)
