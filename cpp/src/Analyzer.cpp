#include "Analyzer.h"

#include <chrono>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
#include <atomic>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <format>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <cstdint>
#include <string>
#include <thread>
#include <type_traits>
#include <algorithm>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <vector>
#include <array>
#include <libdeflate.h>
#include <spdlog/spdlog.h>

#include "Cursor.h"
#include "MappedFile.h"
#include "WorkQueue.h"
#include "BlfTypes.h"
#include "NetTypes.h"

namespace fastrace {


// ratio used to allocate buffer for decompressed data. if insufficient, a reallocation would be triggered
// based on real traces, compressed objects seem to have a compression ratio of 4.5.
constexpr size_t DECOMP_BUFFER_PREALLOC_RATIO = 6;
constexpr uint32_t BLF_LOGG_SIGNATURE = 0x47474f4c;  // "LOGG"
constexpr uint32_t BLF_LOBJ_SIGNATURE = 0x4a424f4c;  // "LOBJ"



// ---------------------------------------------------------------------------
// parseTransport – decode and log TCP/UDP headers from a raw Ethernet frame.
// frame must point to the first byte of the Ethernet wire data (dst MAC).
// ---------------------------------------------------------------------------
static constexpr std::string_view kPTPMsgTypes[16] = {
    "Sync", "Delay_Req", "Pdelay_Req", "Pdelay_Resp",
    "?4",   "?5",        "?6",         "?7",
    "Follow_Up", "Delay_Resp", "Pdelay_Resp_Follow_Up", "Announce",
    "Signaling", "Management", "?14", "?15",
};

static void parsePTP(const char* data, size_t len) {
    if (len < sizeof(PTPHeader)) return;
    const auto* p = reinterpret_cast<const PTPHeader*>(data);
    const uint8_t msgType = p->msgTypeTransSpec & 0x0Fu;
    const auto& ci = p->clockIdentity;
    spdlog::debug("    PTP {} dom={} seq={} src={:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}:{}",
        kPTPMsgTypes[msgType],
        p->domainNumber,
        beToHost16(p->sequenceId),
        ci[0], ci[1], ci[2], ci[3], ci[4], ci[5], ci[6], ci[7],
        beToHost16(p->sourcePortNumber));
}

// Format an IPv6 address as eight colon-separated 16-bit groups.
static std::string fmtIPv6(const uint8_t addr[16]) {
    return std::format("{:02x}{:02x}:{:02x}{:02x}:{:02x}{:02x}:{:02x}{:02x}:"
                       "{:02x}{:02x}:{:02x}{:02x}:{:02x}{:02x}:{:02x}{:02x}",
        addr[0],  addr[1],  addr[2],  addr[3],
        addr[4],  addr[5],  addr[6],  addr[7],
        addr[8],  addr[9],  addr[10], addr[11],
        addr[12], addr[13], addr[14], addr[15]);
}

// Decode and log TCP/UDP from a raw Ethernet wire frame (dst MAC first).
static void parseTransport(const char* frame, size_t frameLen) {
    if (frameLen < sizeof(EthernetWireHeader)) return;

    const auto* eth = reinterpret_cast<const EthernetWireHeader*>(frame);
    size_t offset = sizeof(EthernetWireHeader);

    uint16_t etherType = beToHost16(eth->etherType);

    // Strip one VLAN tag (802.1Q or 802.1AD).
    if ((etherType == 0x8100u || etherType == 0x88A8u) && frameLen >= offset + 4) {
        uint16_t inner;
        std::memcpy(&inner, frame + offset + 2, 2);
        etherType = beToHost16(inner);
        offset += 4;
    }

    if (etherType == 0x0800u) {
        // IPv4
        if (frameLen < offset + sizeof(IPv4Header)) return;
        const auto* ip = reinterpret_cast<const IPv4Header*>(frame + offset);
        const size_t ihlBytes = static_cast<size_t>(ip->versionIHL & 0x0Fu) * 4u;
        if (ihlBytes < 20u || frameLen < offset + ihlBytes) return;
        offset += ihlBytes;

        if (ip->protocol == 6u && frameLen >= offset + sizeof(TCPHeader)) {
            const auto* tcp = reinterpret_cast<const TCPHeader*>(frame + offset);
            spdlog::debug("  TCP {}.{}.{}.{}:{} -> {}.{}.{}.{}:{} flags=0x{:02x}",
                ip->srcIP[0], ip->srcIP[1], ip->srcIP[2], ip->srcIP[3],
                beToHost16(tcp->srcPort),
                ip->dstIP[0], ip->dstIP[1], ip->dstIP[2], ip->dstIP[3],
                beToHost16(tcp->dstPort),
                tcp->flags);
        } else if (ip->protocol == 17u && frameLen >= offset + sizeof(UDPHeader)) {
            const auto* udp = reinterpret_cast<const UDPHeader*>(frame + offset);
            const uint16_t sport = beToHost16(udp->srcPort);
            const uint16_t dport = beToHost16(udp->dstPort);
            const uint16_t udpLen = beToHost16(udp->length);
            spdlog::debug("  UDP {}.{}.{}.{}:{} -> {}.{}.{}.{}:{} len={}",
                ip->srcIP[0], ip->srcIP[1], ip->srcIP[2], ip->srcIP[3], sport,
                ip->dstIP[0], ip->dstIP[1], ip->dstIP[2], ip->dstIP[3], dport,
                udpLen > 8u ? udpLen - 8u : 0u);
            if (sport == 319u || sport == 320u || dport == 319u || dport == 320u)
                parsePTP(frame + offset + sizeof(UDPHeader),
                         frameLen - offset - sizeof(UDPHeader));
        }
    } else if (etherType == 0x86DDu) {
        // IPv6 (fixed 40-byte header; skip extension headers until TCP/UDP)
        if (frameLen < offset + sizeof(IPv6Header)) return;
        const auto* ip6 = reinterpret_cast<const IPv6Header*>(frame + offset);
        offset += sizeof(IPv6Header);

        uint8_t nextHdr = ip6->nextHeader;
        // Walk past common extension headers (routing=43, hop-by-hop=0, dest=60).
        while (nextHdr == 0u || nextHdr == 43u || nextHdr == 60u) {
            if (frameLen < offset + 2) return;
            const uint8_t extLen = static_cast<uint8_t>(frame[offset + 1]);
            nextHdr = static_cast<uint8_t>(frame[offset]);
            offset += static_cast<size_t>(extLen + 1) * 8u;
            if (frameLen < offset) return;
        }
        // Fragment header (44) is fixed 8 bytes.
        if (nextHdr == 44u) {
            if (frameLen < offset + 8) return;
            nextHdr = static_cast<uint8_t>(frame[offset]);
            offset += 8u;
        }

        if (nextHdr == 6u && frameLen >= offset + sizeof(TCPHeader)) {
            const auto* tcp = reinterpret_cast<const TCPHeader*>(frame + offset);
            spdlog::debug("  TCP [{}]:{} -> [{}]:{} flags=0x{:02x}",
                fmtIPv6(ip6->srcAddr), beToHost16(tcp->srcPort),
                fmtIPv6(ip6->dstAddr), beToHost16(tcp->dstPort),
                tcp->flags);
        } else if (nextHdr == 17u && frameLen >= offset + sizeof(UDPHeader)) {
            const auto* udp = reinterpret_cast<const UDPHeader*>(frame + offset);
            const uint16_t sport = beToHost16(udp->srcPort);
            const uint16_t dport = beToHost16(udp->dstPort);
            const uint16_t udpLen = beToHost16(udp->length);
            spdlog::debug("  UDP [{}]:{} -> [{}]:{} len={}",
                fmtIPv6(ip6->srcAddr), sport,
                fmtIPv6(ip6->dstAddr), dport,
                udpLen > 8u ? udpLen - 8u : 0u);
            if (sport == 319u || sport == 320u || dport == 319u || dport == 320u)
                parsePTP(frame + offset + sizeof(UDPHeader),
                         frameLen - offset - sizeof(UDPHeader));
        }
    } else if (etherType == 0x88F7u) {
        // PTP directly over Ethernet (no IP/UDP wrapper)
        parsePTP(frame + offset, frameLen - offset);
    }
}

// ---------------------------------------------------------------------------
// processInnerObjects – walk decompressed container payload, decode CAN and
// Ethernet log objects, and accumulate per-call counters.
// headFragOut and tailFragOut will be null when being called by the Stitcher.
//
// Design notes:
//   * Operates entirely on the caller-supplied scratch buffer which is already
//     hot in L3 cache after libdeflate writes it.  Zero extra copies.
//   * Returns counts via out-params so the caller can merge them into a single
//     atomic update – minimising atomic traffic between threads.
// ---------------------------------------------------------------------------
// Forward declarations – both are defined later in this file.
static bool findNextLobj(Cursor& cursor, uint32_t& sigOut);
std::string_view objectTypeName(uint32_t t);

static void processInnerObjects(Analyzer* self, const char* data, size_t dataLen,
                                std::unordered_map<uint32_t, uint64_t>& counts,
                                uint64_t& splitCount,
                                std::vector<char>* headFragOut = nullptr,
                                std::vector<char>* tailFragOut = nullptr) {
  Cursor cur{data, data + dataLen, data};

  BlfObjectHeaderBase base;
  bool firstObject = true;
  while (!cur.eof()) {
    if (!findNextLobj(cur, base.signature)) break;

    if (firstObject) {
      firstObject = false;
      const size_t firstLobjAt = cur.tell() - 4;
      if (firstLobjAt > 0 && headFragOut)
        headFragOut->assign(data, data + firstLobjAt);
    }

    if (!cur.read(reinterpret_cast<char*>(&base) + 4,
                  sizeof(BlfObjectHeaderBase) - 4)) break;

    // -----------------------------------------------------------------------
    // Cross-container split detection.
    //
    // base.objectSize is the total byte length of this LOBJ (header + payload)
    // measured from the start of the "LOBJ" signature.  The object started
    // sizeof(BlfObjectHeaderBase) bytes before the current cursor position.
    //
    //   objectEnd = objectStart + base.objectSize
    //             = (cur.tell() - sizeof(BlfObjectHeaderBase)) + base.objectSize
    //
    // If objectEnd > dataLen the encoder placed the tail of this object into
    // the next compressed container – exactly the split case we want to detect.
    // -----------------------------------------------------------------------
    const size_t objectStart = cur.tell() - sizeof(BlfObjectHeaderBase);
    const size_t objectEnd   = objectStart + base.objectSize;
    if (objectEnd > dataLen) {
      const size_t bytesAvail    = dataLen - objectStart;
      const size_t bytesOverflow = objectEnd - dataLen;
      spdlog::debug(
          "SPLIT: type={} ({}) objectSize={} B  |  {} B in this container, {} B in next",
          base.objectType, objectTypeName(base.objectType),
          base.objectSize, bytesAvail, bytesOverflow);
      ++splitCount;
      if (tailFragOut)
        tailFragOut->assign(data + objectStart, data + dataLen);
      break;  // remainder of this object lives in the next container; stop here
    }

    // The object's payload starts right after the base header.
    // headerSize includes the 4-byte signature but NOT the BlfObjectHeader
    // extension; it equals sizeof(BlfObjectHeaderBase) + sizeof(BlfObjectHeader)
    // for type-1 headers, or may differ for type-2/3.  We read exactly
    // (headerSize - sizeof(BlfObjectHeaderBase)) bytes of extra header,
    // then the remaining payload is (objectSize - headerSize) bytes.
    const size_t extraHdrBytes = base.headerSize > sizeof(BlfObjectHeaderBase)
        ? base.headerSize - sizeof(BlfObjectHeaderBase)
        : 0;
    if (extraHdrBytes > 0) {
      // Peek past the extended header without copying
      if (!cur.skip(extraHdrBytes)) break;
    }

    const size_t payloadBytes = base.objectSize > base.headerSize
        ? base.objectSize - base.headerSize
        : 0;

    switch (static_cast<BLFObjectType>(base.objectType)) {

      case CAN_MESSAGE:
      case CAN_MESSAGE2: {
        if (payloadBytes < sizeof(CANMessage)) {
          spdlog::debug("inner: CAN object too short ({} B)", payloadBytes);
          cur.skip(payloadBytes);
          break;
        }
        CANMessage msg;
        cur.read(&msg, sizeof(CANMessage));
        const size_t surplus = payloadBytes - sizeof(CANMessage);
        if (surplus > 0) cur.skip(surplus);

        ++counts[base.objectType];
        if (self->dumpObjContents) {
          const uint32_t rawId = msg.id;
          const bool     ext   = (rawId >> 31) & 1;
          const uint32_t arbId = ext ? (rawId & 0x1FFFFFFFu) : (rawId & 0x7FFu);
          const uint8_t  dlen  = std::min(msg.dlc, uint8_t{8});
          std::string hex; hex.reserve(dlen * 3);
          for (uint8_t i = 0; i < dlen; ++i)
            hex += std::format("{:02x} ", msg.data[i]);
          spdlog::debug("CAN ch={} id=0x{:x}{} dlc={} data: {}",
                        msg.channel, arbId, ext ? "x" : "", msg.dlc, hex);
        }
        break;
      }

      case ETHERNET_FRAME: {
        if (payloadBytes < sizeof(EthernetFrameHeader)) {
          spdlog::debug("inner: ETH object too short ({} B)", payloadBytes);
          cur.skip(payloadBytes);
          break;
        }
        EthernetFrameHeader eth;
        cur.read(&eth, sizeof(EthernetFrameHeader));
        const char* frameData = cur.pos;   // raw Ethernet wire bytes (dst MAC, src MAC, ...)
        const size_t frameLen = payloadBytes - sizeof(EthernetFrameHeader);
        if (frameLen > 0) cur.skip(frameLen);

        ++counts[base.objectType];
        if (self->dumpObjContents) {
          const size_t dumpLen = std::min(frameLen, size_t{20});
          std::string hex; hex.reserve(dumpLen * 3);
          for (size_t i = 0; i < dumpLen; ++i)
            hex += std::format("{:02x} ", static_cast<uint8_t>(frameData[i]));
          spdlog::debug("ETH ch={} dir={} paylen={} frame: {}",
                        eth.channel, eth.direction, eth.payloadLength, hex);
          parseTransport(frameData, frameLen);
        }
        break;
      }

      case CAN_FD_MESSAGE: {
        if (payloadBytes < sizeof(CANFDMessage)) {
          spdlog::debug("inner: CAN_FD object too short ({} B)", payloadBytes);
          cur.skip(payloadBytes);
          break;
        }
        CANFDMessage msg;
        cur.read(&msg, sizeof(CANFDMessage));
        const char* dataPtr  = cur.pos;  // data[64] follows the fixed header
        const size_t dataAvail = payloadBytes - sizeof(CANFDMessage);
        if (dataAvail > 0) cur.skip(dataAvail);

        ++counts[base.objectType];
        if (self->dumpObjContents) {
          const uint32_t rawId = msg.arbId;
          const bool     ext   = (rawId >> 31) & 1;
          const uint32_t arbId = ext ? (rawId & 0x1FFFFFFFu) : (rawId & 0x7FFu);
          const size_t   dlen  = std::min(static_cast<size_t>(msg.validDataBytes), dataAvail);
          std::string hex; hex.reserve(dlen * 3);
          for (size_t i = 0; i < dlen; ++i)
            hex += std::format("{:02x} ", static_cast<uint8_t>(dataPtr[i]));
          spdlog::debug("CAN_FD ch={} id=0x{:x}{} dlc={} len={} data: {}",
                        msg.channel, arbId, ext ? "x" : "", msg.dlc, msg.validDataBytes, hex);
        }
        break;
      }

      case CAN_FD_MESSAGE_64: {
        if (payloadBytes < sizeof(CANFDMessage64)) {
          spdlog::debug("inner: CAN_FD_64 object too short ({} B)", payloadBytes);
          cur.skip(payloadBytes);
          break;
        }
        CANFDMessage64 msg;
        cur.read(&msg, sizeof(CANFDMessage64));
        const char* dataPtr   = cur.pos;  // data follows (extDataOffset reserved bytes then payload)
        const size_t dataAvail = payloadBytes - sizeof(CANFDMessage64);
        if (dataAvail > 0) cur.skip(dataAvail);

        ++counts[base.objectType];
        if (self->dumpObjContents) {
          const uint32_t rawId = msg.arbId;
          const bool     ext   = (rawId >> 31) & 1;
          const uint32_t arbId = ext ? (rawId & 0x1FFFFFFFu) : (rawId & 0x7FFu);
          const bool     brs   = (msg.flags & 0x1000u) != 0;
          const bool     edl   = (msg.flags & 0x2000u) != 0;
          const size_t   skip  = msg.extDataOffset;  // reserved bytes before actual data
          const size_t   dlen  = (dataAvail > skip)
                                   ? std::min(static_cast<size_t>(msg.validDataBytes), dataAvail - skip)
                                   : 0;
          std::string hex; hex.reserve(dlen * 3);
          for (size_t i = 0; i < dlen; ++i)
            hex += std::format("{:02x} ", static_cast<uint8_t>(dataPtr[skip + i]));
          spdlog::debug("CAN_FD64 ch={} id=0x{:x}{} dlc={} len={} brs={} edl={} data: {}",
                        +msg.channel, arbId, ext ? "x" : "", msg.dlc, msg.validDataBytes,
                        brs, edl, hex);
        }
        break;
      }

      case ETHERNET_FRAME_EX: {
        if (payloadBytes < sizeof(EthernetFrameExHeader)) {
          spdlog::debug("inner: ETH_EX object too short ({} B)", payloadBytes);
          cur.skip(payloadBytes);
          break;
        }
        EthernetFrameExHeader eth;
        cur.read(&eth, sizeof(EthernetFrameExHeader));
        const char* frameData = cur.pos;   // raw Ethernet wire bytes
        const size_t frameLen = payloadBytes - sizeof(EthernetFrameExHeader);
        if (frameLen > 0) cur.skip(frameLen);

        ++counts[base.objectType];
        if (self->dumpObjContents) {
          const size_t dumpLen = std::min(frameLen, size_t{20});
          std::string hex; hex.reserve(dumpLen * 3);
          for (size_t i = 0; i < dumpLen; ++i)
            hex += std::format("{:02x} ", static_cast<uint8_t>(frameData[i]));
          spdlog::debug("ETH_EX ch={} dir={} paylen={} frame: {}",
                        eth.channel, eth.dir, eth.payloadLen, hex);
          parseTransport(frameData, frameLen);
        }
        break;
      }

      case CONTAINER:
        cur.skip(payloadBytes);
        break;

      default:
        if (payloadBytes > 0) cur.skip(payloadBytes);
        ++counts[base.objectType];
        break;
    }
  }
}

// ---------------------------------------------------------------------------
// Per-run timing accumulators
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
// Fragment types and queue for the Stitcher thread (Design F).
// ---------------------------------------------------------------------------
struct Fragment {
  enum class Tag { Head, Tail };
  Tag               tag;
  uint64_t          containerIndex;
  std::vector<char> bytes;
};

struct FragmentQueue {
  void push(Fragment f) {
    { std::lock_guard<std::mutex> lk(mu_); q_.push(std::move(f)); }
    cv_.notify_one();
  }

  std::optional<Fragment> pop() {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [&] { return !q_.empty() || closed_; });
    if (q_.empty()) return std::nullopt;
    Fragment f = std::move(q_.front());
    q_.pop();
    return f;
  }

  void close() {
    { std::lock_guard<std::mutex> lk(mu_); closed_ = true; }
    cv_.notify_all();
  }

private:
  std::queue<Fragment>    q_;
  std::mutex              mu_;
  std::condition_variable cv_;
  bool                    closed_ = false;
};


std::string to_hex(const char* buf, size_t len) {
  std::string result;
  result.reserve(len * 2);
  for (size_t i = 0; i < len; i++) {
    result += std::format("{:02x}", static_cast<unsigned char>(buf[i]));
  }
  return result;
}


std::string to_hex(uint32_t num) {
  return to_hex(reinterpret_cast<const char*>(&num), 4);
}


// ---------------------------------------------------------------------------
// objectTypeName – human-readable label for a BLF object type ID.
// Returns "UNKNOWN_<n>" for any type not in the table.
// ---------------------------------------------------------------------------
std::string_view objectTypeName(uint32_t t) {
  static const auto names = [] {
    std::array<std::string_view, 256> arr{};
    arr[CAN_MESSAGE] = "CAN_MESSAGE";
    arr[CAN_ERROR] = "CAN_ERROR";
    arr[CAN_OVERLOAD] = "CAN_OVERLOAD";
    arr[CAN_STATISTIC] = "CAN_STATISTIC";
    arr[APP_TEXT] = "APP_TEXT";
    arr[CAN_REMOTE_FRAME] = "CAN_REMOTE_FRAME";
    arr[LIN_MESSAGE] = "LIN_MESSAGE";
    arr[CONTAINER] = "CONTAINER";
    arr[LIN_RX_ERROR] = "LIN_RX_ERROR";
    arr[LIN_SEND_ERROR] = "LIN_SEND_ERROR";
    arr[LIN_SLAVE_TIMEOUT] = "LIN_SLAVE_TIMEOUT";
    arr[LIN_NOANS] = "LIN_NOANS";
    arr[LIN_WAKEUP] = "LIN_WAKEUP";
    arr[LIN_SPIKE] = "LIN_SPIKE";
    arr[LIN_DLCINFO] = "LIN_DLCINFO";
    arr[LIN_RCV_ERROR] = "LIN_RCV_ERROR";
    arr[LIN_SYNCERROR] = "LIN_SYNCERROR";
    arr[LIN_BAUDRATE] = "LIN_BAUDRATE";
    arr[LIN_SLEEP] = "LIN_SLEEP";
    arr[LIN_WAKEUP2] = "LIN_WAKEUP2";
    arr[MOST_SPY] = "MOST_SPY";
    arr[MOST_CTRL] = "MOST_CTRL";
    arr[MOST_LIGHTLOCK] = "MOST_LIGHTLOCK";
    arr[MOST_STATISTIC] = "MOST_STATISTIC";
    arr[FLEXRAY_DATA] = "FLEXRAY_DATA";
    arr[FLEXRAY_SYNC] = "FLEXRAY_SYNC";
    arr[CAN_DRIVER_ERROR] = "CAN_DRIVER_ERROR";
    arr[MOST_PKT] = "MOST_PKT";
    arr[MOST_PKT2] = "MOST_PKT2";
    arr[MOST_HWMODE] = "MOST_HWMODE";
    arr[MOST_REG] = "MOST_REG";
    arr[MOST_GENREG] = "MOST_GENREG";
    arr[MOST_NETSTATE] = "MOST_NETSTATE";
    arr[MOST_DATALOST] = "MOST_DATALOST";
    arr[MOST_TRIGGER] = "MOST_TRIGGER";
    arr[FLEXRAY_CYCLE] = "FLEXRAY_CYCLE";
    arr[FLEXRAY_MESSAGE] = "FLEXRAY_MESSAGE";
    arr[LIN_CHECKSUM_INFO] = "LIN_CHECKSUM_INFO";
    arr[LIN_SPIKE_IGNORE] = "LIN_SPIKE_IGNORE";
    arr[LIN_WAKEUP_INFO] = "LIN_WAKEUP_INFO";
    arr[LIN_IN_PROGRESS] = "LIN_IN_PROGRESS";
    arr[LIN_UNEXPECTED_WAKEUP] = "LIN_UNEXPECTED_WAKEUP";
    arr[LIN_SHORT_OR_SLOW_RESPONSE] = "LIN_SHORT_OR_SLOW_RESPONSE";
    arr[LIN_DISTURBANCE_EVENT] = "LIN_DISTURBANCE_EVENT";
    arr[SERIAL_EVENT] = "SERIAL_EVENT";
    arr[OVERRUN_ERROR] = "OVERRUN_ERROR";
    arr[EVENT_COMMENT] = "EVENT_COMMENT";
    arr[WLAN_FRAME] = "WLAN_FRAME";
    arr[WLAN_STATISTIC] = "WLAN_STATISTIC";
    arr[MOST_ECL] = "MOST_ECL";
    arr[SYS_VARIABLE] = "SYS_VARIABLE";
    arr[CAN_ERROR_EXT] = "CAN_ERROR_EXT";
    arr[CAN_DRIVER_ERROR_EXT] = "CAN_DRIVER_ERROR_EXT";
    arr[LIN_LONG_DOM_SIG] = "LIN_LONG_DOM_SIG";
    arr[MOST_150_MESSAGE] = "MOST_150_MESSAGE";
    arr[MOST_150_PKT] = "MOST_150_PKT";
    arr[MOST_ETHERNET_PKT] = "MOST_ETHERNET_PKT";
    arr[MOST_150_MESSAGE_FRAGMENT] = "MOST_150_MESSAGE_FRAGMENT";
    arr[MOST_150_PKT_FRAGMENT] = "MOST_150_PKT_FRAGMENT";
    arr[MOST_ETHERNET_PKT_FRAGMENT] = "MOST_ETHERNET_PKT_FRAGMENT";
    arr[MOST_SYSTEM_EVENT] = "MOST_SYSTEM_EVENT";
    arr[MOST_150_ALLOCTAB] = "MOST_150_ALLOCTAB";
    arr[MOST_50_MESSAGE] = "MOST_50_MESSAGE";
    arr[MOST_50_PKT] = "MOST_50_PKT";
    arr[CAN_MESSAGE2] = "CAN_MESSAGE2";
    arr[LIN_UNEXPECTED_WAKEUP2] = "LIN_UNEXPECTED_WAKEUP2";
    arr[LIN_SHORT_OR_SLOW_RESPONSE3] = "LIN_SHORT_OR_SLOW_RESPONSE3";
    arr[LIN_DISTURBANCE_EVENT2] = "LIN_DISTURBANCE_EVENT2";
    arr[APP_TRIGGER] = "APP_TRIGGER";
    arr[ENV_INTEGER] = "ENV_INTEGER";
    arr[ENV_DOUBLE] = "ENV_DOUBLE";
    arr[ENV_STRING] = "ENV_STRING";
    arr[ENV_DATA] = "ENV_DATA";
    arr[GRAPHICS_OBJECT] = "GRAPHICS_OBJECT";
    arr[GLOBAL_MARKER] = "GLOBAL_MARKER";
    arr[AFDX_FRAME] = "AFDX_FRAME";
    arr[AFDX_STATISTIC] = "AFDX_STATISTIC";
    arr[KLINE_STATUSEVENT] = "KLINE_STATUSEVENT";
    arr[CAN_FD_MESSAGE] = "CAN_FD_MESSAGE";
    arr[CAN_FD_MESSAGE_64] = "CAN_FD_MESSAGE_64";
    arr[ETHERNET_RX_ERROR] = "ETHERNET_RX_ERROR";
    arr[ETHERNET_STATUS] = "ETHERNET_STATUS";
    arr[CAN_FD_ERROR_64] = "CAN_FD_ERROR_64";
    arr[LIN_SHORT_OR_SLOW_RESPONSE2] = "LIN_SHORT_OR_SLOW_RESPONSE2";
    arr[AFDX_STATUS] = "AFDX_STATUS";
    arr[AFDX_BUS_STATISTIC] = "AFDX_BUS_STATISTIC";
    arr[AFDX_ERROR_EVENT] = "AFDX_ERROR_EVENT";
    arr[A429_ERROR] = "A429_ERROR";
    arr[A429_STATUS] = "A429_STATUS";
    arr[A429_BUS_STATISTIC] = "A429_BUS_STATISTIC";
    arr[A429_MESSAGE] = "A429_MESSAGE";
    arr[ETHERNET_STATISTIC] = "ETHERNET_STATISTIC";
    arr[RESERVED_1] = "RESERVED_1";
    arr[RESERVED_2] = "RESERVED_2";
    arr[RESERVED_3] = "RESERVED_3";
    arr[TEST_STRUCTURE] = "TEST_STRUCTURE";
    arr[DIAG_REQUEST_INTERPRETATION] = "DIAG_REQUEST_INTERPRETATION";
    arr[ETHERNET_FRAME] = "ETHERNET_FRAME";
    arr[ETHERNET_FRAME_EX] = "ETHERNET_FRAME_EX";
    arr[ETHERNET_ERROR_EX] = "ETHERNET_ERROR_EX";
    arr[ETHERNET_ERROR_FORWARDED] = "ETHERNET_ERROR_FORWARDED";
    arr[FUNC_BUS] = "FUNC_BUS";
    arr[DATA_LOST_BEGIN] = "DATA_LOST_BEGIN";
    arr[DATA_LOST_END] = "DATA_LOST_END";
    arr[WATER_MARK_EVENT] = "WATER_MARK_EVENT";
    arr[TRIGGER_CONDITION] = "TRIGGER_CONDITION";
    arr[CAN_SETTING_CHANGED] = "CAN_SETTING_CHANGED";
    arr[DISTRIBUTED_OBJECT] = "DISTRIBUTED_OBJECT";
    return arr;
  }();
  if (t < names.size()) return names[t];
  return {};
}


// ---------------------------------------------------------------------------
// findNextLobj – scan cursor forward until the "LOBJ" signature is found,
// handling up to 3 bytes of misalignment (e.g. inter-object padding).
// On success: sigOut == BLF_LOBJ_SIGNATURE, cursor is past the 4-byte sig.
// ---------------------------------------------------------------------------
static bool findNextLobj(Cursor& cursor, uint32_t& sigOut) {
  while (cursor.remaining() >= 4) {
    // casting pointer to uint32 breaks c++ strict aliasing rules and is technically UB.
    // this is still ok on x86-64. on ARM, fallback to memcpy
#if defined(__x86_64__) || defined(_M_X64) || defined(_M_IX86)
    sigOut = *reinterpret_cast<const uint32_t*>(cursor.pos);
#else
    std::memcpy(&sigOut, cursor.pos, 4);
#endif
    if (BLF_LOBJ_SIGNATURE == sigOut) {
      cursor.pos += 4;
      return true;
    }
    cursor.pos += 1;
  }
  return false;
}


// ---------------------------------------------------------------------------
// readFileHeader – validate the LOGG header and advance the cursor past it.
// ---------------------------------------------------------------------------
std::optional<BlfFileHeader> readFileHeader(Cursor& cursor) {
  const size_t fileSize = static_cast<size_t>(cursor.end - cursor.base);
  if (fileSize < sizeof(BlfFileHeader)) {
    spdlog::error("File is too short ({} bytes). stop", fileSize); return std::nullopt;
  }

  BlfFileHeader hdr;
  if (!cursor.read(&hdr, sizeof(BlfFileHeader))) {
    spdlog::error("Failed to read file header"); return std::nullopt;
  }
  if (std::string_view(hdr.signature, sizeof(hdr.signature)) != "LOGG") {
    spdlog::error("File does not contain valid LOGG header"); return std::nullopt;
  }

  spdlog::info("Found valid \"LOGG\" header");
  spdlog::debug("Header size:       {} (0x{:x})", hdr.headerSize, hdr.headerSize);
  spdlog::debug("apiVersion:        {} (0x{:x})", hdr.apiVersion, hdr.apiVersion);
  spdlog::debug("compressionLevel:  {}", hdr.compressionLevel);
  spdlog::debug("version:           {}.{}", hdr.majorVersion, hdr.minorVersion);
  spdlog::debug("compressedSize:    {:.2f} MiB", hdr.compressedSize / 1048576.0);
  spdlog::debug("uncompressedSize:  {:.2f} MiB", hdr.uncompressedSize / 1048576.0);
  spdlog::debug("numObj:            {}", hdr.numObj);

  const size_t padSize = hdr.headerSize - sizeof(BlfFileHeader);
  if (padSize > 0) {
    const char* pad = cursor.peek(padSize);
    if (pad) spdlog::debug("File header pad: {}", to_hex(pad, padSize));
  }
  return hdr;
}


// ---------------------------------------------------------------------------
// runStitcher – dedicated thread that reassembles LOBJs split across two
// adjacent compressed containers.
//
// Consumers push HeadFragment{i} (bytes before first LOBJ in container i) and
// TailFragment{i} (bytes of the incomplete LOBJ at the tail of container i) to
// the FragmentQueue.  The stitcher pairs TailFragment{i} with HeadFragment{i+1}
// to form a complete buffer, then decodes it with processInnerObjects.
// ---------------------------------------------------------------------------
static void runStitcher(Analyzer* self, FragmentQueue& fragQ,
                        std::mutex& countsMu,
                        std::unordered_map<uint32_t, uint64_t>& sharedCounts,
                        uint64_t& sharedSplits) {
  std::unordered_map<uint64_t, std::vector<char>> pendingTails;
  std::unordered_map<uint64_t, std::vector<char>> pendingHeads;
  std::unordered_map<uint32_t, uint64_t> localCounts;
  uint64_t localSplits = 0;
  uint64_t stitched    = 0;

  std::vector<char> localBuf;
  auto assemble = [&](std::vector<char>& tail, std::vector<char>& head) {
    localBuf.clear();
    const auto sizeNeeded = tail.size() + head.size();
    if (localBuf.capacity() < sizeNeeded) localBuf.reserve(sizeNeeded);
    localBuf.insert(localBuf.end(), tail.begin(), tail.end());
    localBuf.insert(localBuf.end(), head.begin(), head.end());
    processInnerObjects(self, localBuf.data(), localBuf.size(), localCounts, localSplits);
    ++stitched;
  };

  while (auto frag = fragQ.pop()) {
    const uint64_t idx = frag->containerIndex;
    if (frag->tag == Fragment::Tag::Tail) {
      auto it = pendingHeads.find(idx + 1);
      if (it != pendingHeads.end()) {
        assemble(frag->bytes, it->second);
        pendingHeads.erase(it);
      } else {
        pendingTails[idx] = std::move(frag->bytes);
      }
    } else {
      auto it = pendingTails.find(idx - 1);
      if (it != pendingTails.end()) {
        assemble(it->second, frag->bytes);
        pendingTails.erase(it);
      } else {
        pendingHeads[idx] = std::move(frag->bytes);
      }
    }
  }

  if (!pendingTails.empty())
    spdlog::warn("Stitcher: {} unmatched tail(s) – split objects lost", pendingTails.size());
  if (!pendingHeads.empty())
    spdlog::debug("Stitcher: {} unmatched head(s) discarded (inter-object padding)",
                  pendingHeads.size());
  spdlog::debug("Stitcher: reassembled {} split object(s)", stitched);

  if (!localCounts.empty() || localSplits > 0) {
    std::lock_guard<std::mutex> lk(countsMu);
    for (const auto& [type, cnt] : localCounts)
      sharedCounts[type] += cnt;
    sharedSplits += localSplits;
  }
}


// ---------------------------------------------------------------------------
// runConsumer – consumer thread entry point.
// Decompresses BLF container payloads popped from the work queue.
// ---------------------------------------------------------------------------
static void runConsumer(Analyzer* self, WorkQueue& queue,
                        FragmentQueue& fragQ,
                        std::atomic<uint64_t>& atomicContainers,
                        std::atomic<uint64_t>& atomicCompressed,
                        std::atomic<uint64_t>& atomicDecompressed,
                        std::mutex& countsMu,
                        std::unordered_map<uint32_t, uint64_t>& sharedCounts,
                        uint64_t& sharedSplits) {
  auto decomp = std::unique_ptr<libdeflate_decompressor, decltype(&libdeflate_free_decompressor)>(
      libdeflate_alloc_decompressor(), libdeflate_free_decompressor);
  if (!decomp) {
    spdlog::error("Failed to allocate libdeflate decompressor");
    return;
  }
  std::vector<char> localBuf;  // per-thread scratch; never freed until exit
  std::unordered_map<uint32_t, uint64_t> localCounts;  // accumulate without contention
  uint64_t localSplitCount = 0;

  // queue::pop will only return with an item (log object) or if the queue was closed.
  // We do not risk prematurely terminating consumers due to starvation
  while (auto item = queue.pop()) {
    atomicContainers.fetch_add(1, std::memory_order_relaxed);
    atomicCompressed.fetch_add(item->compSize, std::memory_order_relaxed);

    if (self->skipDecompress) continue;

    // Grow the local buffer as needed; the buffer stays at the high-water mark
    // for the rest of this thread's lifetime.
    const size_t needed = item->compSize * DECOMP_BUFFER_PREALLOC_RATIO;
    if (localBuf.size() < needed) localBuf.resize(needed);

    // decompress the log object. expand decompression buffer if it is insufficient.
    size_t actualOut = 0;
    libdeflate_result res;
    do {
      res = libdeflate_zlib_decompress(
          decomp.get(),
          item->compData,  item->compSize,
          localBuf.data(), localBuf.size(),
          &actualOut);
      if (LIBDEFLATE_INSUFFICIENT_SPACE == res) {
        localBuf.resize(localBuf.size() * 2);
        spdlog::debug("localBuf grown to {} B", localBuf.size());
      }
    } while (LIBDEFLATE_INSUFFICIENT_SPACE == res);

    if (LIBDEFLATE_SUCCESS == res) {
      atomicDecompressed.fetch_add(actualOut, std::memory_order_relaxed);

      // Parse inner LOBJs while the decompressed data is hot in L3 cache.
      std::vector<char> headFrag, tailFrag;
      uint64_t localSplits = 0;
      processInnerObjects(self, localBuf.data(), actualOut, localCounts, localSplits,
                          &headFrag, &tailFrag);
      if (localSplits) localSplitCount += localSplits;
      if (headFrag.size())
        fragQ.push({Fragment::Tag::Head, item->containerIndex, std::move(headFrag)});
      if (tailFrag.size())
        fragQ.push({Fragment::Tag::Tail, item->containerIndex, std::move(tailFrag)});
    } else {
      spdlog::error("libdeflate failed ({})", static_cast<int>(res));
    }
  }

  // Thread is done: merge local counts into the shared map under a mutex.
  // This happens exactly once per thread, so contention is negligible.
  if (!localCounts.empty() || localSplitCount > 0) {
    std::lock_guard<std::mutex> lk(countsMu);
    for (const auto& [type, cnt] : localCounts)
      sharedCounts[type] += cnt;
    sharedSplits += localSplitCount;
  }
}


// ---------------------------------------------------------------------------
// runProducer – scan mmap'd file sequentially, push payload to work queue.
// ---------------------------------------------------------------------------
static void runProducer(Cursor cursor, WorkQueue& queue) {
  using Clock = std::chrono::steady_clock;
  auto prodStart = Clock::now();
  uint64_t containerIdx = 0;

  BlfObjectHeaderBase base;
  while (!cursor.eof()) {
    // seek to start of next object and read it
    if (!findNextLobj(cursor, base.signature)) break;
    if (!cursor.read(reinterpret_cast<char*>(&base) + 4,
                     sizeof(BlfObjectHeaderBase) - 4)) break;

    spdlog::debug("pipeline: type={} objectSize={}", base.objectType, base.objectSize);

    if (CONTAINER == base.objectType) {
      BlfObjectHeader extHdr;
      if (!cursor.read(reinterpret_cast<char*>(&extHdr),
                       sizeof(BlfObjectHeaderBase))) break;

      const size_t compSize =
          base.objectSize - base.headerSize - sizeof(BlfObjectHeader);
      const char* compData = cursor.peek(compSize);
      if (!compData) break;

      queue.push({compData, compSize, containerIdx++});  // blocks if consumers are behind
    } else {
      // Skip non-CONTAINER object payload
      const size_t remaining = base.objectSize - sizeof(BlfObjectHeaderBase);
      if (!cursor.skip(remaining)) break;
    }
  }

  auto prodEnd = Clock::now();
  const double prodMs = std::chrono::duration<double, std::milli>(prodEnd - prodStart).count();
  spdlog::info("Producer done in {:.2f} ms; waiting for consumers...", prodMs);
}


// ---------------------------------------------------------------------------
// runPipeline – single-pass producer-consumer pipeline.
//
// Why this is faster than the previous two-phase approach:
//
//   Old approach (two phases, sequential):
//     Phase 1  – producer reads all 300 MB cold (mmap page faults): 168 ms
//     Phase 1b – pre-allocates 1.4 GB of output buffers: 22 ms
//     Phase 2  – workers read 300 MB (warm) + write 1444 MB (cold): 188 ms
//     Total DRAM traffic: 300 + 300 + 1444 = 2044 MB → ~5.7 GB/s → 580 MB/s
//
//   New approach (pipelined):
//     Producer scans the mmap'd file sequentially, pushing {ptr, size} items
//     to the work queue.  Consumers pop items and decompress into a small
//     per-thread buffer. The decompression buffer is reused for  all containers
//     on that thread.
//     The reused buffer (~130 KB) should stay in L3 cache; decompressed bytes
//     are (mostly) never written to DRAM. Try to process the decompressed
//     containers before they are evicted from cache.
//
//     Total DRAM traffic: ~300 MB (one cold read pass, overlapped with compute)
//
//     Additionally:
//     - MADV_WILLNEED kicks off async page loading before the producer starts,
//       so the producer finds pages warm in the kernel page cache.
//     - Producers and consumers run concurrently; I/O and compute overlap.
// ---------------------------------------------------------------------------
static void runPipeline(Analyzer* self, Cursor cursor, const BlfFileHeader& hdr,
                        size_t nWorkers) {
  using Clock = std::chrono::steady_clock;

  // Bound the queue to 4× workers: producer stays ahead without excessive
  // getting blocked by memory. Each in-flight item is just a pointer + size (16 B).
  WorkQueue queue(nWorkers * 4);

  // Shared perf counters written atomically by workers
  std::atomic<uint64_t> atomicContainers{0};
  std::atomic<uint64_t> atomicCompressed{0};
  std::atomic<uint64_t> atomicDecompressed{0};
  // Per-type object counts and split detections: accumulated locally per thread,
  // merged once at thread exit under a mutex.
  std::mutex countsMu;
  std::unordered_map<uint32_t, uint64_t> sharedCounts;
  uint64_t sharedSplits = 0;

  // ---- Fragment queue to store parts of split log objects and Stitcher thread ----
  FragmentQueue fragQ;
  std::thread stitcher(runStitcher, self, std::ref(fragQ), std::ref(countsMu),
                       std::ref(sharedCounts), std::ref(sharedSplits));

  // ---- Start consumers ----
  std::vector<std::thread> consumers;
  consumers.reserve(nWorkers);
  for (size_t i = 0; i < nWorkers; ++i) {
    consumers.emplace_back(runConsumer, self, std::ref(queue), std::ref(fragQ),
                           std::ref(atomicContainers), std::ref(atomicCompressed),
                           std::ref(atomicDecompressed), std::ref(countsMu),
                           std::ref(sharedCounts), std::ref(sharedSplits));
  }

  // ---- Producer (runs on calling thread, concurrent with consumers) ----
  // MADV_WILLNEED (set in MappedFile::open) has been pre-loading pages since
  // mmap returned, so many of these touches will hit the page cache.
  runProducer(cursor, queue);

  queue.close();  // this tells consumers that no more items will be pushed
  for (auto& w : consumers) w.join();

  // All consumers have exited; no more fragments will be pushed.
  fragQ.close();
  stitcher.join();

  self->perf.containers        = atomicContainers.load();
  self->perf.compressedBytes   = atomicCompressed.load();
  self->perf.decompressedBytes = atomicDecompressed.load();
  self->perf.objectCounts      = std::move(sharedCounts);
  self->perf.splitObjects      = sharedSplits;
  self->perf.nThreads          = nWorkers;
}


// ---------------------------------------------------------------------------
// processFile – top-level orchestrator
// ---------------------------------------------------------------------------
void Analyzer::processFile(const std::string& filename) {
  using Clock = std::chrono::steady_clock;

  MappedFile mf;
  if (!mf.open(filename)) return;
  spdlog::info("Processing file: {} ({:.2f} MiB)", filename, mf.size / (1024.0 * 1024.0));

  Cursor cursor = mf.cursor();
  auto fileHeader = readFileHeader(cursor);
  if (!fileHeader) return;

  const size_t nWorkers = std::max<size_t>(1, std::thread::hardware_concurrency());
  spdlog::info("Starting pipeline: {} worker thread(s)", nWorkers);

  auto t0 = Clock::now();
  runPipeline(this, cursor, *fileHeader, nWorkers);
  this->perf.pipelineUs = std::chrono::duration_cast<std::chrono::microseconds>(
      Clock::now() - t0).count();
}



} // namespace fastrace
