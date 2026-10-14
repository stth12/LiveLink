#pragma once
#include "shared/protocol/AudioPacket.h"
#include <atomic>
#include <memory>
#include <windows.h>
#include <sddl.h>
#include <string>
#include <windows.h>
namespace ll {
static_assert(std::atomic<uint32_t>::is_always_lock_free);
constexpr uint32_t ipcVersion = 3, slots = 256;
struct alignas(64) Shared {
    std::atomic<uint32_t> magic{0};
    uint32_t version = ipcVersion;
    std::atomic<uint32_t> producer{0}, write{0}, read{0}, drops{0};
    std::atomic<uint32_t> heartbeat{0}, receivers{0}, rate{0}, peakL{0}, peakR{0};
    std::atomic<uint64_t> adminToken{0};
    std::atomic<uint32_t> pin{0};
    std::atomic<uint32_t> servicePid{0};
    std::atomic<uint32_t> namesRevision{0};
    std::array<std::atomic<uint32_t>, 100> receiverNames{};
    std::array<Block, slots> blocks;
    bool push(const Block &b) noexcept {
        auto w = write.load(std::memory_order_relaxed);
        if (w - read.load(std::memory_order_acquire) >= slots) {
            drops.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        blocks[w % slots] = b;
        write.store(w + 1, std::memory_order_release);
        return true;
    }
    bool pop(Block &b) noexcept {
        auto r = read.load(std::memory_order_relaxed);
        if (r == write.load(std::memory_order_acquire))
            return false;
        b = blocks[r % slots];
        read.store(r + 1, std::memory_order_release);
        return true;
    }
};
inline std::wstring mappingName() {
    HANDLE t{};
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t))
        throw std::runtime_error("Cannot read user identity");
    DWORD n = 0;
    GetTokenInformation(t, TokenUser, nullptr, 0, &n);
    std::vector<uint8_t> buf(n);
    if (!GetTokenInformation(t, TokenUser, buf.data(), n, &n)) {
        CloseHandle(t);
        throw std::runtime_error("Cannot read SID");
    }
    LPWSTR sid{};
    if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(buf.data())->User.Sid, &sid)) {
        CloseHandle(t);
        throw std::runtime_error("Cannot format user SID");
    }
    std::wstring name = L"Local\\LiveLink-v3-";
    name += sid;
    LocalFree(sid);
    CloseHandle(t);
    return name;
}
class Mapping {
    HANDLE handle{};
    Shared *ptr{};

  public:
    Mapping(bool create) {
        auto name = mappingName();
        handle = create ? CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                             sizeof(Shared), name.c_str())
                        : OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
        if (!handle)
            return;
        bool fresh = GetLastError() != ERROR_ALREADY_EXISTS;
        ptr =
            static_cast<Shared *>(MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shared)));
        if (ptr && create && fresh) {
            new (ptr) Shared;
            ptr->magic.store(0x4c4c4950, std::memory_order_release);
        }
        if (ptr && (ptr->magic.load(std::memory_order_acquire) != 0x4c4c4950 ||
                    ptr->version != ipcVersion)) {
            UnmapViewOfFile(ptr);
            ptr = nullptr;
        }
    }
    ~Mapping() {
        if (ptr)
            UnmapViewOfFile(ptr);
        if (handle)
            CloseHandle(handle);
    }
    Shared *get() const { return ptr; }
    Mapping(const Mapping &) = delete;
};
} // namespace ll
