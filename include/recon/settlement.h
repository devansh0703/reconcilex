#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "recon/types.h"

namespace recon {

struct SettlementRecord {
    std::string trade_id;
    SettlementStatus status = SettlementStatus::Pending;
    uint64_t last_updated_ns = 0;
    std::string venue;
    double amount = 0.0;
};

struct SettlementSummary {
    int64_t total_settled = 0;
    int64_t total_pending = 0;
    int64_t total_failed = 0;
    int64_t total_buyin = 0;
    double fail_rate = 0.0;
};

class SettlementTracker {
public:
    void track_settlement(const std::string& trade_id,
                          SettlementStatus status,
                          const std::string& venue = "",
                          double amount = 0.0);

    bool update_status(const std::string& trade_id, SettlementStatus new_status);

    std::vector<SettlementRecord> check_fails(uint64_t deadline_ns) const;
    std::vector<SettlementRecord> get_all() const;
    SettlementSummary get_summary(const std::string& date = "") const;

    SettlementRecord* get_record(const std::string& trade_id);
    const SettlementRecord* get_record(const std::string& trade_id) const;

    size_t total_tracked() const { return records_.size(); }

private:
    std::unordered_map<std::string, SettlementRecord> records_;
};

} // namespace recon
