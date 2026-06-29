#pragma once
#include "Detection.h"
#include "ProtocolMessage.h"
#include <memory>
#include <string>
#include <vector>

class Detector {
public:
    virtual ~Detector() = default;

    virtual std::string name() const = 0;
    virtual bool isStateful() const = 0;
    virtual void inspect(const ProtocolMessage& msg) = 0;
    virtual void finalize() = 0;
    virtual void reset() = 0;
    virtual std::unique_ptr<Detector> clone() const = 0;
    virtual void merge(const Detector& other) = 0;

    std::vector<Detection> takeDetections() { return std::move(m_detections); }

protected:
    void emitDetection(const Detection& d) { m_detections.push_back(d); }

    void emitDetection(Detection&& d) { m_detections.push_back(std::move(d)); }

private:
    std::vector<Detection> m_detections;
};
