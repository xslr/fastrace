#pragma once
#include <atomic>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

enum class PacketType : uint8_t { CAN = 0, CAN_FD = 1, CAN_FD64 = 2, ETH = 3, ETH_EX = 4 };

struct StoredPacket {
  uint64_t   timestamp_100ns;  // raw BLF timestamp; unit confirmed 100ns on most recorders
  uint32_t   id;               // CAN arb ID (extended-frame bit already stripped); 0 for ETH
  PacketType type;
  uint8_t    channel;          // 1-based
  uint8_t    dlc;              // CAN DLC; 0 for ETH
  uint8_t    pad;
  uint8_t    data[8];          // first 8 payload bytes
};

class PacketStore {
public:
  static constexpr size_t CAP = 5'000'000;  // scaffold: ~140 MB; not "first N" — arb N per thread

  // Must be called before any push(). Not called on non-serving invocations so the
  // 140 MB resize doesn't hit benchmark/CI runs.
  void init() { packets_.resize(CAP); }

  void push(const StoredPacket& pkt) noexcept {
    const size_t slot = count_.fetch_add(1, std::memory_order_relaxed);
    if (slot < CAP) packets_[slot] = pkt;
  }

  void sort_by_time() {
    const size_t n = size();
    std::sort(packets_.data(), packets_.data() + n,
              [](const StoredPacket& a, const StoredPacket& b) {
                return a.timestamp_100ns < b.timestamp_100ns;
              });
  }

  size_t size() const noexcept {
    return std::min(count_.load(std::memory_order_relaxed), CAP);
  }

  bool capped() const noexcept {
    return count_.load(std::memory_order_relaxed) >= CAP;
  }

  const StoredPacket* data() const noexcept { return packets_.data(); }

private:
  std::vector<StoredPacket> packets_;
  std::atomic<size_t>       count_{0};
};
