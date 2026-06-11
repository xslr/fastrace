#include "ArxmlParser.h"
#include <pugixml.hpp>
#include <spdlog/spdlog.h>
#include <cstring>
#include <cstdlib>
#include <unordered_map>

namespace fastrace {
namespace {

static uint32_t parseU32(const char* s) {
    return (s && *s) ? static_cast<uint32_t>(std::strtoul(s, nullptr, 10)) : 0;
}

static const char* cv(pugi::xml_node n, const char* child) {
    return n.child(child).text().get();
}

// Internal index types -------------------------------------------------------

struct ISignalInfo {
    uint32_t bitLength = 0;
};

struct SignalMapping {
    std::string name;
    std::string iSignalRef;
    uint32_t    startBit    = 0;
    bool        isBigEndian = false;
};

struct IPduInfo {
    uint32_t byteLength = 0;
    std::vector<SignalMapping> mappings;
};

struct CanFrameInfo {
    uint32_t    dlc    = 0;
    std::string pduRef;
};

struct CanFtInfo {
    std::string shortName;
    uint32_t    canId      = 0;
    bool        isExtended = false;
    std::string frameRef;
    std::string cluster;
};

struct EthPtInfo {
    std::string shortName;
    std::string pduRef;
    std::string channel;
};

struct IndexMaps {
    std::unordered_map<std::string, ISignalInfo>  iSignals;
    std::unordered_map<std::string, IPduInfo>     iPdus;
    std::unordered_map<std::string, CanFrameInfo> canFrames;
    std::vector<CanFtInfo>                        frameTriggers;
    std::vector<EthPtInfo>                        ethPduTriggers;
    std::vector<ArEcu>                            ecus;
    std::vector<ArSomeIpService>                  services;
};

// ----------------------------------------------------------------------------

static void indexCanCluster(pugi::xml_node el, const char* clusterName, IndexMaps& maps) {
    for (auto cond : el.child("CAN-CLUSTER-VARIANTS").children("CAN-CLUSTER-CONDITIONAL")) {
        for (auto chan : cond.child("PHYSICAL-CHANNELS").children("CAN-PHYSICAL-CHANNEL")) {
            for (auto ft : chan.child("FRAME-TRIGGERINGS").children("CAN-FRAME-TRIGGERING")) {
                CanFtInfo info;
                info.shortName  = cv(ft, "SHORT-NAME");
                info.canId      = parseU32(cv(ft, "IDENTIFIER"));
                info.cluster    = clusterName;
                const char* mode = cv(ft, "CAN-ADDRESSING-MODE");
                info.isExtended = mode && std::strcmp(mode, "EXTENDED") == 0;
                info.frameRef   = ft.child("FRAME-REF").text().get();
                maps.frameTriggers.push_back(std::move(info));
            }
        }
    }
}

static void indexEthernetCluster(pugi::xml_node el, IndexMaps& maps) {
    for (auto cond : el.child("ETHERNET-CLUSTER-VARIANTS")
                        .children("ETHERNET-CLUSTER-CONDITIONAL")) {
        for (auto chan : cond.child("PHYSICAL-CHANNELS")
                            .children("ETHERNET-PHYSICAL-CHANNEL")) {
            const char* chanName = cv(chan, "SHORT-NAME");
            for (auto pt : chan.child("PDU-TRIGGERINGS").children("PDU-TRIGGERING")) {
                auto pduRef = pt.child("I-PDU-REF");
                if (std::strcmp(pduRef.attribute("DEST").value(), "I-SIGNAL-I-PDU") != 0)
                    continue;
                EthPtInfo info;
                info.shortName = cv(pt, "SHORT-NAME");
                info.pduRef    = pduRef.text().get();
                info.channel   = chanName;
                maps.ethPduTriggers.push_back(std::move(info));
            }
        }
    }
}

static void indexPackage(pugi::xml_node pkg, const std::string& parentPath, IndexMaps& maps) {
    const char* nameStr = cv(pkg, "SHORT-NAME");
    if (!nameStr || !*nameStr) return;
    const std::string path = parentPath + "/" + nameStr;

    for (auto el : pkg.child("ELEMENTS").children()) {
        const char* tag  = el.name();
        const char* sn   = cv(el, "SHORT-NAME");
        const std::string elPath = path + "/" + sn;

        if (std::strcmp(tag, "I-SIGNAL") == 0) {
            ISignalInfo info;
            info.bitLength = parseU32(cv(el, "LENGTH"));
            maps.iSignals[elPath] = info;
        }
        else if (std::strcmp(tag, "I-SIGNAL-I-PDU") == 0) {
            IPduInfo info;
            info.byteLength = parseU32(cv(el, "LENGTH"));
            for (auto m : el.child("I-SIGNAL-TO-PDU-MAPPINGS")
                            .children("I-SIGNAL-TO-I-PDU-MAPPING")) {
                SignalMapping sm;
                sm.name         = cv(m, "SHORT-NAME");
                sm.iSignalRef   = m.child("I-SIGNAL-REF").text().get();
                sm.startBit     = parseU32(cv(m, "START-POSITION"));
                const char* bo  = cv(m, "PACKING-BYTE-ORDER");
                sm.isBigEndian  = bo && std::strcmp(bo, "MOST-SIGNIFICANT-BYTE-FIRST") == 0;
                info.mappings.push_back(std::move(sm));
            }
            maps.iPdus[elPath] = std::move(info);
        }
        else if (std::strcmp(tag, "CAN-FRAME") == 0) {
            CanFrameInfo info;
            info.dlc = parseU32(cv(el, "FRAME-LENGTH"));
            // PDU-TO-FRAME-MAPPINGS > PDU-TO-FRAME-MAPPING > PDU-REF
            for (auto m : el.child("PDU-TO-FRAME-MAPPINGS")
                            .children("PDU-TO-FRAME-MAPPING")) {
                auto pduRef = m.child("PDU-REF");
                const char* dest = pduRef.attribute("DEST").value();
                if (std::strcmp(dest, "I-SIGNAL-I-PDU") == 0) {
                    info.pduRef = pduRef.text().get();
                    break;
                }
            }
            maps.canFrames[elPath] = std::move(info);
        }
        else if (std::strcmp(tag, "CAN-CLUSTER") == 0) {
            indexCanCluster(el, sn, maps);
        }
        else if (std::strcmp(tag, "ETHERNET-CLUSTER") == 0) {
            indexEthernetCluster(el, maps);
        }
        else if (std::strcmp(tag, "ECU-INSTANCE") == 0) {
            maps.ecus.push_back(ArEcu{sn});
        }
    }

    for (auto sub : pkg.child("AR-PACKAGES").children("AR-PACKAGE"))
        indexPackage(sub, path, maps);
}

static void collectServices(pugi::xml_node root, IndexMaps& maps) {
    for (auto n : root.select_nodes("//PROVIDED-SERVICE-INSTANCE")) {
        ArSomeIpService svc;
        svc.name = cv(n.node(), "SHORT-NAME");
        svc.serviceId = static_cast<uint16_t>(
            parseU32(cv(n.node(), "SERVICE-IDENTIFIER")));
        if (!svc.name.empty() && svc.serviceId != 0)
            maps.services.push_back(std::move(svc));
    }
}

static ArDatabase buildDatabase(IndexMaps& maps) {
    ArDatabase db;
    db.ecus            = std::move(maps.ecus);
    db.someipServices  = std::move(maps.services);

    for (auto& ft : maps.frameTriggers) {
        ArMessage msg;
        msg.name       = ft.shortName;
        msg.canId      = ft.canId;
        msg.isExtended = ft.isExtended;
        msg.cluster    = ft.cluster;

        auto frameIt = maps.canFrames.find(ft.frameRef);
        if (frameIt == maps.canFrames.end()) {
            db.messages.push_back(std::move(msg));
            continue;
        }
        msg.dlc = frameIt->second.dlc;

        if (frameIt->second.pduRef.empty()) {
            db.messages.push_back(std::move(msg));
            continue;
        }

        auto pduIt = maps.iPdus.find(frameIt->second.pduRef);
        if (pduIt == maps.iPdus.end()) {
            db.messages.push_back(std::move(msg));
            continue;
        }

        for (const auto& sm : pduIt->second.mappings) {
            ArSignal sig;
            sig.name        = sm.name;
            sig.startBit    = sm.startBit;
            sig.isBigEndian = sm.isBigEndian;
            auto sigIt = maps.iSignals.find(sm.iSignalRef);
            if (sigIt != maps.iSignals.end())
                sig.bitLength = sigIt->second.bitLength;
            msg.signalDefs.push_back(std::move(sig));
        }

        db.messages.push_back(std::move(msg));
    }

    // Ethernet PDU-TRIGGERINGs
    for (auto& pt : maps.ethPduTriggers) {
        ArMessage msg;
        msg.name    = pt.shortName;
        msg.cluster = pt.channel;
        msg.busType = ArBusType::ETHERNET;

        auto pduIt = maps.iPdus.find(pt.pduRef);
        if (pduIt == maps.iPdus.end()) {
            db.messages.push_back(std::move(msg));
            continue;
        }
        msg.dlc = pduIt->second.byteLength;

        for (const auto& sm : pduIt->second.mappings) {
            ArSignal sig;
            sig.name        = sm.name;
            sig.startBit    = sm.startBit;
            sig.isBigEndian = sm.isBigEndian;
            auto sigIt = maps.iSignals.find(sm.iSignalRef);
            if (sigIt != maps.iSignals.end())
                sig.bitLength = sigIt->second.bitLength;
            msg.signalDefs.push_back(std::move(sig));
        }

        db.messages.push_back(std::move(msg));
    }

    db.buildIndex();
    return db;
}

} // anonymous namespace

ArDatabase ArxmlParser::parseFile(const std::string& path) {
    pugi::xml_document doc;
    pugi::xml_parse_result res = doc.load_file(path.c_str());
    if (!res) {
        spdlog::error("ArxmlParser: '{}': {}", path, res.description());
        return {};
    }

    IndexMaps maps;
    auto autosar = doc.child("AUTOSAR");
    for (auto pkg : autosar.child("AR-PACKAGES").children("AR-PACKAGE"))
        indexPackage(pkg, "", maps);
    collectServices(autosar, maps);

    spdlog::info("ArxmlParser: '{}' → {} signals, {} PDUs, {} CAN triggers, {} ETH triggers, {} ECUs, {} services",
        path,
        maps.iSignals.size(), maps.iPdus.size(),
        maps.frameTriggers.size(), maps.ethPduTriggers.size(),
        maps.ecus.size(), maps.services.size());

    return buildDatabase(maps);
}

ArDatabase ArxmlParser::parseFiles(const std::vector<std::string>& paths) {
    ArDatabase merged;
    for (const auto& p : paths) {
        auto db = parseFile(p);
        for (auto& m : db.messages)       merged.messages.push_back(std::move(m));
        for (auto& e : db.ecus)           merged.ecus.push_back(std::move(e));
        for (auto& s : db.someipServices) merged.someipServices.push_back(std::move(s));
    }
    merged.buildIndex();
    return merged;
}

} // namespace fastrace
