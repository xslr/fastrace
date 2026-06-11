#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fastrace {

struct RecentFileEntry {
    std::string path;
    std::string filename;
    int64_t     sizeBytes    = -1;  // -1 = file not accessible
    int64_t     modTimeUnix  = 0;   // seconds since Unix epoch; 0 = not accessible
};

class RecentFiles {
public:
    static std::string defaultDbPath();

    explicit RecentFiles(std::string dbPath = defaultDbPath());
    ~RecentFiles();

    RecentFiles(const RecentFiles&) = delete;
    RecentFiles& operator=(const RecentFiles&) = delete;

    // Record that a file was opened. Bumps it to top of recency list.
    // Prunes the table to at most 10 entries after insertion.
    void addFile(const std::string& path);

    // Returns up to `limit` entries ordered most-recent-first.
    // Performs a live stat() per entry for size and mtime.
    std::vector<RecentFileEntry> getRecent(int limit = 10) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fastrace
