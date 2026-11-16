#pragma once

#include <sqlite3.h>
#include <string>

namespace db_internal {

constexpr int kBusyTimeoutMs = 3000;

inline std::string ColumnText(sqlite3_stmt* stmt, int column) {
    const unsigned char* text = sqlite3_column_text(stmt, column);
    if (text == nullptr) {
        return {};
    }
    return reinterpret_cast<const char*>(text);
}

class StatementGuard {
public:
    explicit StatementGuard(sqlite3_stmt* stmt) : stmt_(stmt) {}
    ~StatementGuard() {
        if (stmt_ != nullptr) {
            sqlite3_finalize(stmt_);
        }
    }
    StatementGuard(const StatementGuard&) = delete;
    StatementGuard& operator=(const StatementGuard&) = delete;

private:
    sqlite3_stmt* stmt_;
};

}