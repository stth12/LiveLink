#pragma once
// Included after Control so all actions use its asynchronous command queue.
class SettingsPanel : public juce::Component {
    Control &control;
    juce::ComboBox buffer, interfaces, packets;
    juce::ToggleButton reconnect{"Default automatic reconnection"}, logging{"Diagnostic logging"};
    juce::TextButton save{"SAVE SETTINGS"}, restart{"RESTART BACKGROUND PROCESS"};
    juce::Label text;
    juce::StringArray addresses;

  public:
    explicit SettingsPanel(Control &c) : control(c) {
        for (auto *x : std::initializer_list<juce::Component *>{
                 &buffer, &interfaces, &packets, &reconnect, &logging, &save, &restart, &text})
            addAndMakeVisible(x);
        auto state = c.state();
        auto config = state["settings"];
        buffer.addItemList(
            {"Ultra low / 8 ms base", "Balanced / 20 ms base", "Stable / 40 ms base"}, 1);
        buffer.setSelectedId(int(config["defaultMode"]) + 1);
        packets.addItem("Network packets / 32 source frames", 32);
        packets.addItem("Network packets / 64 source frames", 64);
        packets.addItem("Network packets / 128 source frames", 128);
        packets.setSelectedId(int(config["packetFrames"]));
        addresses.add("0.0.0.0");
        for (auto &ip : juce::IPAddress::getAllAddresses(false))
            addresses.addIfNotAlreadyThere(ip.toString());
        for (int i = 0; i < addresses.size(); ++i)
            interfaces.addItem(i == 0 ? "All network interfaces" : addresses[i], i + 1);
        auto index = addresses.indexOf(config["bindAddress"].toString());
        interfaces.setSelectedId(std::max(0, index) + 1);
        reconnect.setToggleState(bool(config["autoReconnect"]), juce::dontSendNotification);
        logging.setToggleState(bool(config["logging"]), juce::dontSendNotification);
        text.setText("Default buffer / Network interface\nInterface changes "
                     "require restarting the background process.\nRestarting "
                     "disconnects current receivers.\nDark / neon pink theme. "
                     "Windows startup is configured in the installer.\nPairing "
                     "credentials expire when the background process exits.",
                     juce::dontSendNotification);
        save.onClick = [this] {
            auto req = obj();
            prop(req, "op", "settings");
            prop(req, "defaultMode", buffer.getSelectedId() - 1);
            prop(req, "autoReconnect", reconnect.getToggleState());
            prop(req, "logging", logging.getToggleState());
            prop(req, "packetFrames", packets.getSelectedId());
            prop(req, "bindAddress", addresses[interfaces.getSelectedId() - 1]);
            control.send(req);
        };
        restart.onClick = [this] {
            control.command("shutdown");
            juce::Timer::callAfterDelay(1800, [] { launchService(); });
        };
        setSize(580, 402);
    }
    void resized() override {
        auto r = getLocalBounds().reduced(24);
        text.setBounds(r.removeFromTop(128));
        buffer.setBounds(r.removeFromTop(32));
        r.removeFromTop(10);
        interfaces.setBounds(r.removeFromTop(32));
        r.removeFromTop(8);
        packets.setBounds(r.removeFromTop(32));
        reconnect.setBounds(r.removeFromTop(34));
        logging.setBounds(r.removeFromTop(34));
        auto bottom = r.removeFromBottom(36);
        save.setBounds(bottom.removeFromLeft(155));
        restart.setBounds(bottom.reduced(8, 0));
    }
};
