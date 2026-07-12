#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "recon/types.h"
#include "recon/trade_record.h"
#include "recon/confirmation.h"

struct sqlite3;

namespace recon {

class TradeStore {
public:
    explicit TradeStore(const std::string& db_path = ":memory:");
    ~TradeStore();

    TradeStore(const TradeStore&) = delete;
    TradeStore& operator=(const TradeStore&) = delete;
    TradeStore(TradeStore&&) noexcept;
    TradeStore& operator=(TradeStore&&) noexcept;

    bool add_internal(const TradeRecord& record);
    bool add_confirmation(const BrokerConfirmation& confirm);

    std::vector<TradeRecord> query_internal(const std::string& date = "",
                                            const std::string& venue = "",
                                            const std::string& symbol = "") const;

    std::vector<BrokerConfirmation> query_confirmations(const std::string& date = "",
                                                        const std::string& venue = "",
                                                        const std::string& symbol = "") const;

    int64_t get_internal_count() const;
    int64_t get_confirmation_count() const;

    void begin_transaction();
    void commit();
    void rollback();

private:
    void init_schema();
    sqlite3* db_ = nullptr;
};

} // namespace recon
