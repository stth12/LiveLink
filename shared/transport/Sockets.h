#pragma once
#include <stdexcept>
#include <string>
#include <utility>
#include <winsock2.h>
#include <ws2tcpip.h>
namespace ll {
struct Network {
    Network() {
        WSADATA d{};
        if (WSAStartup(MAKEWORD(2, 2), &d))
            throw std::runtime_error("WSAStartup failed");
    }
    ~Network() { WSACleanup(); }
};
struct Socket {
    SOCKET s = INVALID_SOCKET;
    Socket() = default;
    explicit Socket(SOCKET x) : s(x) {}
    ~Socket() { close(); }
    Socket(const Socket &) = delete;
    Socket(Socket &&o) noexcept : s(std::exchange(o.s, INVALID_SOCKET)) {};
    void close() {
        if (s != INVALID_SOCKET) {
            closesocket(s);
            s = INVALID_SOCKET;
        }
    }
};
inline bool sendText(SOCKET s, const std::string &x) {
    size_t n = 0;
    while (n < x.size()) {
        int r = send(s, x.data() + n, int(x.size() - n), 0);
        if (r <= 0)
            return false;
        n += r;
    }
    return true;
}
inline SOCKET connectLocal() {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(49320);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    DWORD ms = 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&ms, sizeof(ms));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char *)&ms, sizeof(ms));
    if (connect(s, (sockaddr *)&a, sizeof(a))) {
        closesocket(s);
        return INVALID_SOCKET;
    }
    return s;
}
inline std::string readLine(SOCKET s) {
    std::string r;
    char c;
    while (r.size() < 8192) {
        int n = recv(s, &c, 1, 0);
        if (n != 1)
            return {};
        if (c == '\n')
            return r;
        r += c;
    }
    return {};
}
} // namespace ll
