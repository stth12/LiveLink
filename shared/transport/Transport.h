#pragma once
#include "shared/transport/Sockets.h"
#include <span>
namespace ll {
class Transport {
  public:
    virtual ~Transport() = default;
    virtual bool available() const noexcept = 0;
    virtual bool send(std::span<const uint8_t>, const sockaddr_in &) noexcept = 0;
};
class WiFiTransport final : public Transport {
    Socket socket{::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)};

  public:
    WiFiTransport() {
        if (socket.s == INVALID_SOCKET)
            throw std::runtime_error("Cannot create UDP transport");
        u_long enabled = 1;
        if (ioctlsocket(socket.s, FIONBIO, &enabled))
            throw std::runtime_error("Cannot configure nonblocking UDP");
    }
    bool available() const noexcept override { return true; }
    void bindSource(const in_addr &address) {
        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_addr = address;
        if (bind(socket.s, reinterpret_cast<const sockaddr *>(&local), sizeof(local)))
            throw std::runtime_error("Cannot bind UDP to the selected interface");
    }
    bool send(std::span<const uint8_t> bytes, const sockaddr_in &address) noexcept override {
        return sendto(socket.s, reinterpret_cast<const char *>(bytes.data()), int(bytes.size()), 0,
                      reinterpret_cast<const sockaddr *>(&address),
                      sizeof(address)) == int(bytes.size());
    }
};
// Deliberately unavailable until real bidirectional Windows/iOS USB validation.
class USBTransport final : public Transport {
  public:
    bool available() const noexcept override { return false; }
    bool send(std::span<const uint8_t>, const sockaddr_in &) noexcept override { return false; }
};
} // namespace ll
