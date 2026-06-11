#include "RecentFiles.h"
#include <sqlite3.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <chrono>

#ifndef _WIN32
#include <sys/stat.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#endif

namespace fastrace {

struct RecentFiles::Impl {
    sqlite3* db = nullptr;
};

static void statEntry(const std::string& path, RecentFileEntry& e) {
#ifndef _WIN32
    struct stat st{};
    if (::stat(path.c_str(), &st) == 0) {
        e.sizeBytes   = static_cast<int64_t>(st.st_size);
        e.modTimeUnix = static_cast<int64_t>(st.st_mtime);
    }
#else
    struct __stat64 st{};
    if (::_stat64(path.c_str(), &st) == 0) {
        e.sizeBytes   = static_cast<int64_t>(st.st_size);
        e.modTimeUnix = static_cast<int64_t>(st.st_mtime);
    }
#endif
}

std::string RecentFiles::defaultDbPath() {
    namespace fs = std::filesystem;
    std::string dir;
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    dir = appdata ? (std::string(appdata) + "\\fastrace") : ".";
#else
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && xdg[0]) {
        dir = std::string(xdg) + "/fastrace";
    } else {
        const char* home = std::getenv("HOME");
        dir = home ? (std::string(home) + "/.local/share/fastrace") : ".";
    }
#endif
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir + "/recent.db";
}

RecentFiles::RecentFiles(std::string dbPath)
    : impl_(std::make_unique<Impl>())
{
    // Ensure parent directory exists
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(fs::path(dbPath).parent_path(), ec);

    int rc = sqlite3_open(dbPath.c_str(), &impl_->db);
    if (rc != SQLITE_OK) {
        spdlog::error("RecentFiles: sqlite3_open({}) failed: {}", dbPath,
                      sqlite3_errmsg(impl_->db));
        sqlite3_close(impl_->db);
        impl_->db = nullptr;
        return;
    }

    const char* schema =
        "CREATE TABLE IF NOT EXISTS recent_files ("
        "  path      TEXT PRIMARY KEY,"
        "  opened_at INTEGER NOT NULL DEFAULT 0"
        ");";

    char* errMsg = nullptr;
    rc = sqlite3_exec(impl_->db, schema, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        spdlog::error("RecentFiles: schema creation failed: {}", errMsg);
        sqlite3_free(errMsg);
    }
}

RecentFiles::~RecentFiles() {
    if (impl_ && impl_->db) {
        sqlite3_close(impl_->db);
        impl_->db = nullptr;
    }
}

void RecentFiles::addFile(const std::string& path) {
    if (!impl_->db) return;

    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    sqlite3_stmt* stmt = nullptr;
    const char* upsert =
        "INSERT INTO recent_files (path, opened_at) VALUES (?, ?) "
        "ON CONFLICT(path) DO UPDATE SET opened_at = excluded.opened_at;";
    if (sqlite3_prepare_v2(impl_->db, upsert, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(now));
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    } else {
        spdlog::error("RecentFiles: addFile prepare failed: {}",
                      sqlite3_errmsg(impl_->db));
    }

    // Keep only the 10 most recently opened entries
    const char* prune =
        "DELETE FROM recent_files WHERE path NOT IN "
        "(SELECT path FROM recent_files ORDER BY opened_at DESC LIMIT 10);";
    sqlite3_exec(impl_->db, prune, nullptr, nullptr, nullptr);
}

std::vector<RecentFileEntry> RecentFiles::getRecent(int limit) const {
    std::vector<RecentFileEntry> result;
    if (!impl_->db) return result;

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT path FROM recent_files ORDER BY opened_at DESC LIMIT ?;";
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        spdlog::error("RecentFiles: getRecent prepare failed: {}",
                      sqlite3_errmsg(impl_->db));
        return result;
    }

    sqlite3_bind_int(stmt, 1, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* raw = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (!raw) continue;

        RecentFileEntry e;
        e.path = raw;
        e.filename = std::filesystem::path(raw).filename().string();
        statEntry(e.path, e);
        result.push_back(std::move(e));
    }
    sqlite3_finalize(stmt);
    return result;
}

} // namespace fastrace
