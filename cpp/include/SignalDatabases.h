#pragma once
#include <memory>
#include <string>
#include <vector>

namespace fastrace {

class SignalDatabases {
public:
    static std::string defaultDbPath();

    explicit SignalDatabases(std::string dbPath = defaultDbPath());
    ~SignalDatabases();

    SignalDatabases(const SignalDatabases&) = delete;
    SignalDatabases& operator=(const SignalDatabases&) = delete;

    void addDatabase(const std::string& path);
    void removeDatabase(const std::string& path);
    std::vector<std::string> getActiveDatabases() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fastrace
