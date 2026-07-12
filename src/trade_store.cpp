#include "recon/trade_store.h"
#include <sqlite3.h>
#include <stdexcept>
#include <sstream>

namespace recon {

TradeStore::TradeStore(const std::string& db_path) {
    int rc = sqlite3_open(db_path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to open database: " + std::string(sqlite3_errmsg(db_)));
    }
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
    init_schema();
}

TradeStore::~TradeStore() {
    if (db_) sqlite3_close(db_);
}

TradeStore::TradeStore(TradeStore&& other) noexcept : db_(other.db_) { other.db_ = nullptr; }
TradeStore& TradeStore::operator=(TradeStore&& other) noexcept {
    if (this != &other) { if (db_) sqlite3_close(db_); db_ = other.db_; other.db_ = nullptr; }
    return *this;
}

void TradeStore::init_schema() {
    const char* schema = R"SQL(
        CREATE TABLE IF NOT EXISTS internal_trades (
            trade_id TEXT NOT NULL,
            timestamp_ns INTEGER NOT NULL,
            symbol TEXT NOT NULL,
            side INTEGER NOT NULL,
            quantity INTEGER NOT NULL,
            price REAL NOT NULL,
            venue TEXT NOT NULL,
            strategy TEXT DEFAULT '',
            account TEXT DEFAULT '',
            status TEXT DEFAULT 'active',
            date TEXT GENERATED ALWAYS AS (substr(trade_id, 1, 10)) STORED,
            PRIMARY KEY (trade_id, venue)
        );
        CREATE INDEX IF NOT EXISTS idx_internal_date ON internal_trades(date);
        CREATE INDEX IF NOT EXISTS idx_internal_symbol ON internal_trades(symbol);

        CREATE TABLE IF NOT EXISTS confirmations (
            confirm_id TEXT NOT NULL,
            trade_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            side INTEGER NOT NULL,
            quantity INTEGER NOT NULL,
            price REAL NOT NULL,
            venue TEXT NOT NULL,
            status TEXT DEFAULT 'confirmed',
            fees REAL DEFAULT 0.0,
            timestamp_ns INTEGER NOT NULL,
            date TEXT GENERATED ALWAYS AS (substr(confirm_id, 1, 10)) STORED,
            PRIMARY KEY (confirm_id, venue)
        );
        CREATE INDEX IF NOT EXISTS idx_confirm_trade ON confirmations(trade_id);
        CREATE INDEX IF NOT EXISTS idx_confirm_date ON confirmations(date);
    )SQL";
    char* err = nullptr;
    sqlite3_exec(db_, schema, nullptr, nullptr, &err);
    if (err) {
        std::string error = err;
        sqlite3_free(err);
        throw std::runtime_error("Schema creation failed: " + error);
    }
}

bool TradeStore::add_internal(const TradeRecord& record) {
    const char* sql = R"(
        INSERT OR IGNORE INTO internal_trades
        (trade_id, timestamp_ns, symbol, side, quantity, price, venue, strategy, account, status)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, record.trade_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(record.timestamp_ns));
    sqlite3_bind_text(stmt, 3, record.symbol.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, static_cast<int>(record.side));
    sqlite3_bind_int64(stmt, 5, record.quantity);
    sqlite3_bind_double(stmt, 6, record.price);
    sqlite3_bind_text(stmt, 7, record.venue.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, record.strategy.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, record.account.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10, record.status.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    bool inserted = sqlite3_changes(db_) > 0;
    sqlite3_finalize(stmt);
    return inserted;
}

bool TradeStore::add_confirmation(const BrokerConfirmation& confirm) {
    const char* sql = R"(
        INSERT OR IGNORE INTO confirmations
        (confirm_id, trade_id, symbol, side, quantity, price, venue, status, fees, timestamp_ns)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, confirm.confirm_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, confirm.trade_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, confirm.symbol.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, static_cast<int>(confirm.side));
    sqlite3_bind_int64(stmt, 5, confirm.quantity);
    sqlite3_bind_double(stmt, 6, confirm.price);
    sqlite3_bind_text(stmt, 7, confirm.venue.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, confirm.status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 9, confirm.fees);
    sqlite3_bind_int64(stmt, 10, static_cast<sqlite3_int64>(confirm.timestamp_ns));
    sqlite3_step(stmt);
    bool inserted = sqlite3_changes(db_) > 0;
    sqlite3_finalize(stmt);
    return inserted;
}

static void bind_optional_filter(sqlite3_stmt* stmt, int idx, const std::string& val) {
    if (val.empty()) {
        sqlite3_bind_null(stmt, idx);
    } else {
        sqlite3_bind_text(stmt, idx, val.c_str(), -1, SQLITE_TRANSIENT);
    }
}

std::vector<TradeRecord> TradeStore::query_internal(const std::string& date,
                                                     const std::string& venue,
                                                     const std::string& symbol) const {
    std::string sql = "SELECT trade_id, timestamp_ns, symbol, side, quantity, price, venue, strategy, account, status FROM internal_trades WHERE 1=1";
    if (!date.empty())   sql += " AND date = ?";
    if (!venue.empty())  sql += " AND venue = ?";
    if (!symbol.empty()) sql += " AND symbol = ?";

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    int idx = 1;
    if (!date.empty())   sqlite3_bind_text(stmt, idx++, date.c_str(), -1, SQLITE_TRANSIENT);
    if (!venue.empty())  sqlite3_bind_text(stmt, idx++, venue.c_str(), -1, SQLITE_TRANSIENT);
    if (!symbol.empty()) sqlite3_bind_text(stmt, idx++, symbol.c_str(), -1, SQLITE_TRANSIENT);

    std::vector<TradeRecord> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TradeRecord r;
        r.trade_id    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        r.timestamp_ns = static_cast<uint64_t>(sqlite3_column_int64(stmt, 1));
        r.symbol      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        r.side        = static_cast<Side>(sqlite3_column_int(stmt, 3));
        r.quantity    = sqlite3_column_int64(stmt, 4);
        r.price       = sqlite3_column_double(stmt, 5);
        r.venue       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        auto strat = sqlite3_column_text(stmt, 7);
        r.strategy    = strat ? reinterpret_cast<const char*>(strat) : "";
        auto acct = sqlite3_column_text(stmt, 8);
        r.account     = acct ? reinterpret_cast<const char*>(acct) : "";
        auto stat = sqlite3_column_text(stmt, 9);
        r.status      = stat ? reinterpret_cast<const char*>(stat) : "";
        results.push_back(std::move(r));
    }
    sqlite3_finalize(stmt);
    return results;
}

std::vector<BrokerConfirmation> TradeStore::query_confirmations(const std::string& date,
                                                                 const std::string& venue,
                                                                 const std::string& symbol) const {
    std::string sql = "SELECT confirm_id, trade_id, symbol, side, quantity, price, venue, status, fees, timestamp_ns FROM confirmations WHERE 1=1";
    if (!date.empty())   sql += " AND date = ?";
    if (!venue.empty())  sql += " AND venue = ?";
    if (!symbol.empty()) sql += " AND symbol = ?";

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    int idx = 1;
    if (!date.empty())   sqlite3_bind_text(stmt, idx++, date.c_str(), -1, SQLITE_TRANSIENT);
    if (!venue.empty())  sqlite3_bind_text(stmt, idx++, venue.c_str(), -1, SQLITE_TRANSIENT);
    if (!symbol.empty()) sqlite3_bind_text(stmt, idx++, symbol.c_str(), -1, SQLITE_TRANSIENT);

    std::vector<BrokerConfirmation> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        BrokerConfirmation c;
        c.confirm_id  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        c.trade_id    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        c.symbol      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        c.side        = static_cast<Side>(sqlite3_column_int(stmt, 3));
        c.quantity    = sqlite3_column_int64(stmt, 4);
        c.price       = sqlite3_column_double(stmt, 5);
        c.venue       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        auto stat = sqlite3_column_text(stmt, 7);
        c.status      = stat ? reinterpret_cast<const char*>(stat) : "";
        c.fees        = sqlite3_column_double(stmt, 8);
        c.timestamp_ns = static_cast<uint64_t>(sqlite3_column_int64(stmt, 9));
        results.push_back(std::move(c));
    }
    sqlite3_finalize(stmt);
    return results;
}

int64_t TradeStore::get_internal_count() const {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM internal_trades", -1, &stmt, nullptr);
    sqlite3_step(stmt);
    int64_t count = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}

int64_t TradeStore::get_confirmation_count() const {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM confirmations", -1, &stmt, nullptr);
    sqlite3_step(stmt);
    int64_t count = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}

void TradeStore::begin_transaction() { sqlite3_exec(db_, "BEGIN", nullptr, nullptr, nullptr); }
void TradeStore::commit() { sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr); }
void TradeStore::rollback() { sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr); }

} // namespace recon
