#include "recon/settlement.h"
#include <chrono>
#include <algorithm>

namespace recon {

using Clock = std::chrono::system_clock;

static uint64_t now_ns() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count()
    );
}

void SettlementTracker::track_settlement(const std::string& trade_id,
                                          SettlementStatus status,
                                          const std::string& venue,
                                          double amount) {
    SettlementRecord rec;
    rec.trade_id = trade_id;
    rec.status = status;
    rec.last_updated_ns = now_ns();
    rec.venue = venue;
    rec.amount = amount;
    records_[trade_id] = rec;
}

bool SettlementTracker::update_status(const std::string& trade_id, SettlementStatus new_status) {
    auto it = records_.find(trade_id);
    if (it == records_.end()) return false;

    auto& rec = it->second;
    // Validate lifecycle: can only move forward, or from Failed to BuyIn
    if (rec.status == SettlementStatus::Settled || rec.status == SettlementStatus::BuyIn) return false;
    if (new_status == SettlementStatus::Pending && rec.status != SettlementStatus::Pending) return false;
    if (new_status == SettlementStatus::Failed && rec.status == SettlementStatus::Settled) return false;

    rec.status = new_status;
    rec.last_updated_ns = now_ns();
    return true;
}

std::vector<SettlementRecord> SettlementTracker::check_fails(uint64_t deadline_ns) const {
    std::vector<SettlementRecord> fails;
    for (const auto& [id, rec] : records_) {
        if (rec.status == SettlementStatus::Pending || rec.status == SettlementStatus::Instructed) {
            if (rec.last_updated_ns < deadline_ns) {
                fails.push_back(rec);
            }
        }
    }
    return fails;
}

std::vector<SettlementRecord> SettlementTracker::get_all() const {
    std::vector<SettlementRecord> all;
    all.reserve(records_.size());
    for (const auto& [_, rec] : records_) all.push_back(rec);
    return all;
}

SettlementSummary SettlementTracker::get_summary(const std::string& /*date*/) const {
    SettlementSummary s;
    for (const auto& [_, rec] : records_) {
        switch (rec.status) {
            case SettlementStatus::Settled:    s.total_settled++; break;
            case SettlementStatus::Pending:    s.total_pending++; break;
            case SettlementStatus::Instructed: s.total_pending++; break;
            case SettlementStatus::Failed:     s.total_failed++; break;
            case SettlementStatus::BuyIn:      s.total_buyin++; break;
        }
    }
    int64_t total = s.total_settled + s.total_pending + s.total_failed + s.total_buyin;
    s.fail_rate = total > 0 ? static_cast<double>(s.total_failed) / total : 0.0;
    return s;
}

SettlementRecord* SettlementTracker::get_record(const std::string& trade_id) {
    auto it = records_.find(trade_id);
    return it != records_.end() ? &it->second : nullptr;
}

const SettlementRecord* SettlementTracker::get_record(const std::string& trade_id) const {
    auto it = records_.find(trade_id);
    return it != records_.end() ? &it->second : nullptr;
}

} // namespace recon
