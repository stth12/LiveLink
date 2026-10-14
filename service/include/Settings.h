#pragma once
#include "shared/transport/Sockets.h"
#include <juce_core/juce_core.h>
#include <mutex>
class Settings {
    juce::File file = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                          .getChildFile("Live Link/preferences.json");
    std::mutex mutex;
    juce::var values;

  public:
    std::atomic<int> mode{1};
    std::atomic<unsigned> packetFrames{128};
    std::atomic<bool> reconnect{true}, logging{false};
    Settings() {
        values = juce::JSON::parse(file.loadFileAsString());
        if (!values.isObject())
            values = juce::var(new juce::DynamicObject);
        auto *o = values.getDynamicObject();
        if (!o->hasProperty("bindAddress"))
            o->setProperty("bindAddress", "0.0.0.0");
        if (o->hasProperty("defaultMode"))
            mode = std::clamp(int(values["defaultMode"]), 0, 2);
        if (o->hasProperty("autoReconnect"))
            reconnect = bool(values["autoReconnect"]);
        if (o->hasProperty("logging"))
            logging = bool(values["logging"]);
        auto frames = int(values["packetFrames"]);
        if (frames == 32 || frames == 64 || frames == 128)
            packetFrames = unsigned(frames);
    }
    juce::var snapshot() {
        std::lock_guard lock(mutex);
        auto v = values.clone();
        v.getDynamicObject()->setProperty("defaultMode", mode.load());
        v.getDynamicObject()->setProperty("autoReconnect", reconnect.load());
        v.getDynamicObject()->setProperty("logging", logging.load());
        v.getDynamicObject()->setProperty("packetFrames", int(packetFrames.load()));
        return v;
    }
    bool save(const juce::var &req) {
        std::lock_guard lock(mutex);
        if (req.hasProperty("packetFrames")) {
            auto n = int(req["packetFrames"]);
            if (n != 32 && n != 64 && n != 128)
                return false;
            packetFrames = unsigned(n);
        }
        auto *o = values.getDynamicObject();
        if (req.hasProperty("bindAddress")) {
            auto text = req["bindAddress"].toString();
            in_addr address{};
            if (InetPtonA(AF_INET, text.toRawUTF8(), &address) != 1)
                return false;
            o->setProperty("bindAddress", text);
        }
        if (req.hasProperty("defaultMode"))
            mode = std::clamp(int(req["defaultMode"]), 0, 2);
        if (req.hasProperty("autoReconnect"))
            reconnect = bool(req["autoReconnect"]);
        if (req.hasProperty("logging"))
            logging = bool(req["logging"]);
        o->setProperty("defaultMode", mode.load());
        o->setProperty("autoReconnect", reconnect.load());
        o->setProperty("logging", logging.load());
        o->setProperty("packetFrames", int(packetFrames.load()));
        file.getParentDirectory().createDirectory();
        return file.replaceWithText(juce::JSON::toString(values));
    }
    void log(const juce::String &message) {
        if (logging) {
            std::lock_guard lock(mutex);
            auto f = file.getSiblingFile("diagnostics.log");
            file.getParentDirectory().createDirectory();
            if (f.getSize() > 2 * 1024 * 1024)
                f.replaceWithText("");
            f.appendText(juce::Time::getCurrentTime().toISO8601(true) + " " + message + "\n");
        }
    }
};
