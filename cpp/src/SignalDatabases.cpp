#include "SignalDatabases.h"
#include <sqlite3.h>
#include <spdlog/spdlog.h>
#include <filesystem>

namespace fastrace {

struct SignalDatabases::Impl {
    sqlite3* db = nullptr;
};

std::string SignalDatabases::defaultDbPath() {
    namespace fs = std::filesystem;
    std::string dir;
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    dir = appdata ? (std::string(appdata) + "\\fastrace") : ".";
#else
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && xdg[0]) dir = std::string(xdg) + "/fastrace";
    else {
        const char* home = std::getenv("HOME");
        dir = home ? (std::string(home) + "/.local/share/fastrace") : ".";
    }
#endif
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir + "/recent.db";
}

SignalDatabases::SignalDatabases(std::string dbPath)
    : impl_(std::make_unique<Impl>())
{
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(fs::path(dbPath).parent_path(), ec);

    int rc = sqlite3_open(dbPath.c_str(), &impl_->db);
    if (rc != SQLITE_OK) {
        spdlog::error("SignalDatabases: open failed: {}", sqlite3_errmsg(impl_->db));
        sqlite3_close(impl_->db);
        impl_->db = nullptr;
        return;
    }

    const char* schema =
        "CREATE TABLE IF NOT EXISTS signal_databases ("
        "  path TEXT PRIMARY KEY"
        ");";
    char* err = nullptr;
    if (sqlite3_exec(impl_->db, schema, nullptr, nullptr, &err) != SQLITE_OK) {
        spdlog::error("SignalDatabases: schema: {}", err);
        sqlite3_free(err);
    }
}

SignalDatabases::~SignalDatabases() {
    if (impl_ && impl_->db) {
        sqlite3_close(impl_->db);
        impl_->db = nullptr;
    }
}

void SignalDatabases::addDatabase(const std::string& path) {
    if (!impl_->db) return;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT OR IGNORE INTO signal_databases (path) VALUES (?);";
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void SignalDatabases::removeDatabase(const std::string& path) {
    if (!impl_->db) return;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "DELETE FROM signal_databases WHERE path = ?;";
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

std::vector<std::string> SignalDatabases::getActiveDatabases() const {
    std::vector<std::string> result;
    if (!impl_->db) return result;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT path FROM signal_databases ORDER BY path;";
    if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* raw = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (raw) result.push_back(raw);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

} // namespace fastrace
