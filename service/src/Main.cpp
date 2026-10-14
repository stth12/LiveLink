#include "service/include/Settings.h"
#include "shared/ipc/SharedMemory.h"
#include "shared/transport/Transport.h"
#include <algorithm>
#include <bcrypt.h>
#include <iostream>
#include <juce_core/juce_core.h>
#include <mutex>
#include <thread>
#include <windns.h>
#include <iphlpapi.h>
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "iphlpapi.lib")
using namespace ll;
using juce::var;
static std::atomic<bool> quitting{false};
template <class F> struct ScopeExit {
    F action;
    ~ScopeExit() { action(); }
};
static BOOL WINAPI onSignal(DWORD) {
    quitting = true;
    return TRUE;
}
static uint64_t random64() {
    uint64_t n{};
    if (BCryptGenRandom(nullptr, (PUCHAR)&n, sizeof(n), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
        throw std::runtime_error("Random generator failed");
    return n ? n : 1;
}
static var object() { return var(new juce::DynamicObject); }
static void set(var &v, const char *k, var x) { v.getDynamicObject()->setProperty(k, x); }
static void set(var &v, const char *k, const std::string &x) { set(v, k, juce::String(x)); }
static void set(var &v, const char *k, const char *x) { set(v, k, juce::String(x)); }
static std::string json(const var &v) { return juce::JSON::toString(v, true).toStdString() + "\n"; }
struct Client {
    uint64_t token{};
    std::string name, id;
    sockaddr_in destination{};
    std::atomic<float> volume{1};
    std::atomic<bool> mute{false}, mono{false}, connected{true};
    std::atomic<uint32_t> sent{0}, errors{0}, loss{0}, underruns{0}, occupancy{0};
    std::atomic<float> jitter{0};
    std::atomic<float> level{0};
    std::atomic<bool> hasTelemetry{false};
    std::atomic<int> mode{1};
};
struct Discovery {
    struct Context {
        DNS_SERVICE_REGISTER_REQUEST request{};
        DNS_SERVICE_CANCEL cancel{};
        PDNS_SERVICE_INSTANCE instance{};
        std::atomic<DWORD> status{ERROR_IO_PENDING};
        HANDLE completed = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        ~Context() {
            if (instance)
                DnsServiceFreeInstance(instance);
            if (completed)
                CloseHandle(completed);
        }
    };
    std::unique_ptr<Context> context = std::make_unique<Context>();
    DWORD status() const { return context->status.load(); }
    static void WINAPI callback(DWORD s, void *context, PDNS_SERVICE_INSTANCE result) {
        auto *self = static_cast<Context *>(context);
        self->status = s;
        if (result)
            DnsServiceFreeInstance(result);
        SetEvent(self->completed);
    }
    explicit Discovery(ULONG interfaceIndex) {
        auto &c = *context;
        wchar_t host[256];
        DWORD n = 256;
        GetComputerNameW(host, &n);
        std::wstring h = host;
        h += L".local";
        std::wstring service = host;
        service += L"._livelink._tcp.local";
        c.instance = DnsServiceConstructInstance(service.c_str(), h.c_str(), nullptr, nullptr,
                                                 49320, 0, 0, 0, nullptr, nullptr);
        if (!c.instance) {
            c.status = GetLastError();
            SetEvent(c.completed);
            return;
        }
        c.request.Version = DNS_QUERY_REQUEST_VERSION1;
        c.request.InterfaceIndex = interfaceIndex;
        c.request.pServiceInstance = c.instance;
        c.request.pRegisterCompletionCallback = callback;
        c.request.pQueryContext = &c;
        auto result = DnsServiceRegister(&c.request, &c.cancel);
        if (result != DNS_REQUEST_PENDING) {
            c.status = result;
            SetEvent(c.completed);
        }
    }
    ~Discovery() {
        auto &c = *context;
        if (c.status == ERROR_IO_PENDING)
            DnsServiceRegisterCancel(&c.cancel);
        if (WaitForSingleObject(c.completed, 5000) != WAIT_OBJECT_0) {
            context.release();
            return;
        }
        if (c.status == ERROR_SUCCESS) {
            ResetEvent(c.completed);
            auto result = DnsServiceDeRegister(&c.request, nullptr);
            if (result == DNS_REQUEST_PENDING &&
                WaitForSingleObject(c.completed, 5000) != WAIT_OBJECT_0) {
                context.release();
                return;
            }
        }
        // On a stuck OS callback only its small context survives until process
        // exit; never free storage still referenced by the asynchronous DNS API.
    }
};
class Server {
    Settings settings;
    juce::String bindAddress;
    Mapping mapping{true};
    Shared *shared = mapping.get();
    Socket listener{socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
    Socket adminListener;
    WiFiTransport wifi;
    USBTransport usb;
    std::mutex mutex;
    std::vector<std::shared_ptr<Client>> clients;
    std::atomic<uint32_t> sequence{0}, blocks{0};
    std::atomic<DWORD> audioAt{0};
    std::atomic<unsigned> sessions{0}, failedPairings{0};
    std::atomic<DWORD> pairingLockUntil{0};
    std::unique_ptr<Discovery> discovery;

  public:
    Server() {
        if (!shared)
            throw std::runtime_error("Shared memory unavailable");
        shared->adminToken = random64();
        shared->servicePid = uint32_t(GetCurrentProcessId());
        auto pin = std::to_string(10000000 + random64() % 90000000);
        shared->pin = uint32_t(std::stoul(pin));
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_port = htons(49320);
        bindAddress = settings.snapshot()["bindAddress"].toString();
        if (InetPtonA(AF_INET, bindAddress.toRawUTF8(), &a.sin_addr) != 1)
            throw std::runtime_error("Invalid configured IPv4 address");
        BOOL exclusive = TRUE;
        setsockopt(listener.s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&exclusive,
                   sizeof(exclusive));
        if (listener.s == INVALID_SOCKET || bind(listener.s, (sockaddr *)&a, sizeof(a)) ||
            listen(listener.s, 32))
            throw std::runtime_error("Cannot listen on TCP 49320 (another service running?)");
        wifi.bindSource(a.sin_addr);
        ULONG interfaceIndex = 0;
        if (a.sin_addr.s_addr != htonl(INADDR_ANY) && a.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
            adminListener.s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            sockaddr_in local = a;
            local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            setsockopt(adminListener.s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&exclusive,
                       sizeof(exclusive));
            if (bind(adminListener.s, (sockaddr *)&local, sizeof(local)) ||
                listen(adminListener.s, 16))
                throw std::runtime_error("Cannot bind the loopback administration listener");
            ULONG bytes = 16384;
            std::vector<uint8_t> adapters(bytes);
            auto result = GetAdaptersAddresses(
                AF_INET, 0, nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES *>(adapters.data()),
                &bytes);
            if (result == ERROR_BUFFER_OVERFLOW) {
                adapters.resize(bytes);
                result = GetAdaptersAddresses(
                    AF_INET, 0, nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES *>(adapters.data()),
                    &bytes);
            }
            if (result == NO_ERROR)
                for (auto *adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(adapters.data());
                     adapter; adapter = adapter->Next)
                    for (auto *u = adapter->FirstUnicastAddress; u; u = u->Next)
                        if (u->Address.lpSockaddr->sa_family == AF_INET &&
                            reinterpret_cast<sockaddr_in *>(u->Address.lpSockaddr)
                                    ->sin_addr.s_addr == a.sin_addr.s_addr)
                            interfaceIndex = adapter->IfIndex;
        }
        discovery = std::make_unique<Discovery>(interfaceIndex);
    }
    var state() {
        auto v = object();
        set(v, "ok", true);
        set(v, "service", "online");
        set(v, "activeReceivers", int(shared->receivers.load()));
        set(v, "settings", settings.snapshot());
        set(v, "activeBindAddress", bindAddress);
        set(v, "pin", juce::String(shared->pin.load()));
        set(v, "sampleRate", int(shared->rate.load()));
        set(v, "streaming", GetTickCount() - audioAt.load() < 1000 && blocks > 0);
        set(v, "blocks", int(blocks.load()));
        set(v, "captureDrops", int(shared->drops.load()));
        set(v, "peakL", std::bit_cast<float>(shared->peakL.load()));
        set(v, "peakR", std::bit_cast<float>(shared->peakR.load()));
        set(v, "mdnsStatus", int(discovery->status()));
        set(v, "usb", "unavailable: hardware proof of concept required");
        juce::Array<var> list;
        std::lock_guard guard(mutex);
        for (auto &c : clients) {
            auto d = object();
            set(d, "id", c->id);
            set(d, "name", c->name);
            set(d, "connected", c->connected.load());
            set(d, "hasTelemetry", c->hasTelemetry.load());
            set(d, "streaming",
                c->connected.load() && c->sent.load() > 0 &&
                    GetTickCount() - audioAt.load() < 1000);
            set(d, "volume", c->volume.load());
            set(d, "mute", c->mute.load());
            set(d, "mono", c->mono.load());
            set(d, "mode", c->mode.load());
            set(d, "packets", int(c->sent.load()));
            set(d, "sendErrors", int(c->errors.load()));
            set(d, "loss", int(c->loss.load()));
            set(d, "underruns", int(c->underruns.load()));
            set(d, "occupancy", int(c->occupancy.load()));
            set(d, "jitterMs", c->jitter.load());
            set(d, "bufferEstimateMs",
                shared->rate.load() > 0 ? double(c->occupancy.load()) * 1000 / shared->rate.load()
                                        : 0.0);
            set(d, "level", GetTickCount() - audioAt.load() < 1000 ? c->level.load() : 0.f);
            list.add(d);
        }
        set(v, "devices", list);
        return v;
    }
    void session(SOCKET socket, sockaddr_in peer) {
        Socket connection(socket);
        DWORD ms = 1000;
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&ms, sizeof(ms));
        setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&ms, sizeof(ms));
        std::shared_ptr<Client> client;
        ScopeExit cleanup{[&] {
            if (client) {
                client->connected = false;
                std::lock_guard guard(mutex);
                std::erase(clients, client);
            }
            sessions--;
        }};
        bool admin = false;
        std::string pending;
        DWORD last = GetTickCount();
        while (!quitting && GetTickCount() - last < 10000) {
            char buf[2048];
            int n = recv(socket, buf, sizeof(buf), 0);
            if (n == 0)
                break;
            if (n < 0) {
                if (WSAGetLastError() == WSAETIMEDOUT)
                    continue;
                break;
            }
            last = GetTickCount();
            pending.append(buf, n);
            if (pending.size() > 8192)
                break;
            size_t end;
            while ((end = pending.find('\n')) != std::string::npos) {
                auto line = pending.substr(0, end);
                pending.erase(0, end + 1);
                var req = juce::JSON::parse(line);
                var reply = object();
                set(reply, "ok", false);
                auto op = req["op"].toString();
                if (op == "admin" && peer.sin_addr.s_addr == htonl(INADDR_LOOPBACK) &&
                    req["token"].toString() ==
                        juce::String::toHexString((juce::int64)shared->adminToken.load())) {
                    admin = true;
                    reply = state();
                } else if (admin) {
                    if (op == "status")
                        reply = state();
                    else if (op == "settings") {
                        if (settings.save(req))
                            reply = state();
                        else
                            set(reply, "error",
                                "Could not save settings; check IPv4 address and file "
                                "permissions");
                    } else if (op == "shutdown") {
                        set(reply, "ok", true);
                        quitting = true;
                    } else if (op == "rotatePin") {
                        auto pin = std::to_string(10000000 + random64() % 90000000);
                        shared->pin = uint32_t(std::stoul(pin));
                        failedPairings = 0;
                        pairingLockUntil = 0;
                        reply = state();
                    } else if (op == "device" || op == "all") {
                        std::lock_guard guard(mutex);
                        for (auto &c : clients)
                            if (op == "all" || req["id"].toString().toStdString() == c->id) {
                                apply(*c, req);
                            }
                        set(reply, "ok", true);
                    }
                } else if (op == "pair" && !client) {
                    auto now = GetTickCount();
                    bool locked =
                        pairingLockUntil.load() != 0 && int32_t(pairingLockUntil.load() - now) > 0;
                    if (!locked && req["pin"].toString() == juce::String(shared->pin.load())) {
                        int port = int(req["port"]);
                        std::lock_guard guard(mutex);
                        if (port >= 1024 && port <= 65535 && clients.size() < 16) {
                            client = std::make_shared<Client>();
                            client->token = random64();
                            client->mode = settings.mode.load();
                            client->id =
                                juce::String::toHexString((juce::int64)client->token).toStdString();
                            client->name = req["name"].toString().substring(0, 80).toStdString();
                            client->destination = peer;
                            client->destination.sin_port = htons((u_short)port);
                            clients.push_back(client);
                            set(reply, "ok", true);
                            set(reply, "token", client->id);
                            set(reply, "frames", 128);
                            set(reply, "version", 1);
                            set(reply, "mode", settings.mode.load());
                            set(reply, "autoReconnect", settings.reconnect.load());
                        }
                    } else {
                        if (failedPairings.fetch_add(1) + 1 >= 5)
                            pairingLockUntil = now + 30000;
                        set(reply, "error", "Pairing denied; after five failures wait 30 seconds");
                        settings.log("Pairing denied");
                    }
                } else if (client) {
                    if (op == "ping" || op == "device") {
                        apply(*client, req);
                        set(reply, "ok", true);
                        set(reply, "connected", client->connected.load());
                        set(reply, "volume", client->volume.load());
                        set(reply, "mute", client->mute.load());
                        set(reply, "mono", client->mono.load());
                        set(reply, "mode", client->mode.load());
                        if (req.hasProperty("loss"))
                            client->loss = uint32_t(std::max(0, int(req["loss"])));
                        if (req.hasProperty("underruns"))
                            client->underruns = uint32_t(std::max(0, int(req["underruns"])));
                        if (req.hasProperty("occupancy"))
                            client->hasTelemetry = true;
                        if (req.hasProperty("occupancy"))
                            client->occupancy =
                                uint32_t(std::clamp(int(req["occupancy"]), 0, 65536));
                        if (req.hasProperty("jitterMs"))
                            client->jitter = std::clamp(float(req["jitterMs"]), 0.f, 10000.f);
                    } else if (op == "disconnect") {
                        client->connected = false;
                        set(reply, "ok", true);
                        sendText(socket, json(reply));
                        goto done;
                    }
                }
                if (!sendText(socket, json(reply)))
                    goto done;
            }
        }
    done:
        return;
    }
    static void apply(Client &c, const var &r) {
        if (r.hasProperty("volume")) {
            float gain = float(r["volume"]);
            if (std::isfinite(gain))
                c.volume = std::clamp(gain, 0.f, 1.f);
        }
        if (r.hasProperty("mute"))
            c.mute = bool(r["mute"]);
        if (r.hasProperty("mono"))
            c.mono = bool(r["mono"]);
        if (r.hasProperty("connected"))
            c.connected = bool(r["connected"]);
        if (r.hasProperty("mode"))
            c.mode = std::clamp(int(r["mode"]), 0, 2);
    }
    void run() {
        std::jthread audio([this] {
            Block b;
            DWORD namesTick = 0;
            while (!quitting) {
                shared->heartbeat = GetTickCount();
                std::vector<std::shared_ptr<Client>> receivers;
                {
                    std::lock_guard guard(mutex);
                    receivers = clients;
                }
                unsigned active = 0;
                for (auto &c : receivers)
                    if (c->connected)
                        ++active;
                shared->receivers = active;
                if (GetTickCount() - namesTick > 500) {
                    namesTick = GetTickCount();
                    std::array<uint8_t, 400> names{};
                    unsigned row = 0;
                    for (auto &c : receivers)
                        if (c->connected && row < 5) {
                            memcpy(names.data() + row * 80, c->name.data(),
                                   std::min(size_t(79), c->name.size()));
                            row++;
                        }
                    shared->namesRevision.fetch_add(1);
                    for (unsigned i = 0; i < 100; ++i) {
                        uint32_t chunk;
                        memcpy(&chunk, names.data() + i * 4, 4);
                        shared->receiverNames[i] = chunk;
                    }
                    shared->namesRevision.fetch_add(1);
                }
                bool any = false;
                while (shared->pop(b)) {
                    any = true;
                    blocks++;
                    audioAt = GetTickCount();
                    auto frameCount = settings.packetFrames.load();
                    for (unsigned offset = 0; offset < 128; offset += frameCount) {
                        auto seq = sequence.fetch_add(1);
                        for (auto &c : receivers) {
                            if (!c->connected)
                                continue;
                            auto packet = encode(b, c->token, seq, c->volume, c->mono, c->mute,
                                                 frameCount, offset);
                            float peak = 0;
                            for (unsigned i = 0; i < frameCount * 2; ++i)
                                peak = std::max(peak, std::abs(std::bit_cast<float>(get32(
                                                          packet.data() + headerBytes + i * 4))));
                            c->level = peak;
                            if (wifi.send(std::span(packet).first(headerBytes + frameCount * 8),
                                          c->destination))
                                c->sent++;
                            else
                                c->errors++;
                        }
                    }
                }
                auto pid = shared->producer.load();
                if (pid) {
                    HANDLE p = OpenProcess(SYNCHRONIZE, FALSE, pid);
                    if (p) {
                        if (WaitForSingleObject(p, 0) == WAIT_OBJECT_0) {
                            shared->producer = 0;
                            shared->read = shared->write.load();
                        }
                        CloseHandle(p);
                    }
                }
                if (!any)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
        struct Worker {
            std::shared_ptr<std::atomic<bool>> done;
            std::jthread thread;
        };
        std::vector<Worker> workers;
        settings.log("Background process started on " + bindAddress + ":49320");
        std::cout << "Live Link service listening on TCP 49320. Pairing PIN is "
                     "shown in desktop.\n";
        while (!quitting) {
            std::erase_if(workers, [](auto &w) { return w.done->load(); });
            fd_set read;
            FD_ZERO(&read);
            FD_SET(listener.s, &read);
            if (adminListener.s != INVALID_SOCKET)
                FD_SET(adminListener.s, &read);
            timeval t{0, 200000};
            if (select(0, &read, nullptr, nullptr, &t) <= 0)
                continue;
            sockaddr_in peer{};
            int len = sizeof(peer);
            auto incoming = FD_ISSET(listener.s, &read) ? listener.s : adminListener.s;
            auto s = accept(incoming, (sockaddr *)&peer, &len);
            if (s == INVALID_SOCKET)
                continue;
            if (sessions >= 32) {
                closesocket(s);
                continue;
            }
            sessions++;
            auto done = std::make_shared<std::atomic<bool>>(false);
            workers.push_back({done, std::jthread([this, s, peer, done] {
                                   try {
                                       session(s, peer);
                                   } catch (...) {
                                       std::cerr << "Control session failed; receiver removed\n";
                                   }
                                   *done = true;
                               })});
        }
        shared->heartbeat = 0;
        shared->receivers = 0;
    }
};
int main(int argc, char **argv) {
    try {
        Network network;
        if (argc > 1 && (std::string(argv[1]) == "--stop" || std::string(argv[1]) == "--status" ||
                         std::string(argv[1]) == "--command")) {
            Mapping map(false);
            if (auto *s = map.get()) {
                Socket socket(connectLocal());
                if (socket.s == INVALID_SOCKET)
                    return 0;
                auto req = object();
                set(req, "op", "admin");
                set(req, "token", juce::String::toHexString((juce::int64)s->adminToken.load()));
                sendText(socket.s, json(req));
                auto status = readLine(socket.s);
                if (std::string(argv[1]) == "--status") {
                    std::cout << status << '\n';
                    return 0;
                }
                if (std::string(argv[1]) == "--command") {
                    if (argc < 3)
                        return 3;
                    auto command = juce::JSON::parse(argv[2]);
                    if (!command.isObject())
                        return 3;
                    sendText(socket.s, json(command));
                    auto reply = readLine(socket.s);
                    std::cout << reply << '\n';
                    return bool(juce::JSON::parse(reply)["ok"]) ? 0 : 4;
                }
                set(req, "op", "shutdown");
                HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, s->servicePid.load());
                sendText(socket.s, json(req));
                readLine(socket.s);
                socket.close();
                if (process) {
                    auto result = WaitForSingleObject(process, 12000);
                    CloseHandle(process);
                    if (result != WAIT_OBJECT_0)
                        return 5;
                }
            }
            return 0;
        }
        SetConsoleCtrlHandler(onSignal, TRUE);
        auto name = mappingName() + L"-service";
        HANDLE singleton = CreateMutexW(nullptr, TRUE, name.c_str());
        if (!singleton || GetLastError() == ERROR_ALREADY_EXISTS) {
            if (singleton)
                CloseHandle(singleton);
            return 2;
        }
        Server server;
        server.run();
        CloseHandle(singleton);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
