#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "recon/trade_record.h"
#include "recon/confirmation.h"
#include "recon/types.h"

namespace recon {

struct MatchResult {
    MatchState state = MatchState::Matched;
    std::string internal_trade_id;
    std::string broker_trade_id;
    std::string symbol;
    std::string details;
    double price_diff = 0.0;
    int64_t qty_diff = 0;
};

struct MatchingConfig {
    double price_tolerance = 0.01;
    int64_t qty_tolerance = 1;
    bool auto_resolve = true;
};

class MatchingEngine {
public:
    explicit MatchingEngine(MatchingConfig config = {});

    MatchResult match_single(const TradeRecord& internal, const BrokerConfirmation& confirm) const;
    std::vector<MatchResult> batch_match(const std::vector<TradeRecord>& internals,
                                         const std::vector<BrokerConfirmation>& confirms) const;

    MatchResult fuzzy_match(const TradeRecord& internal,
                            const std::vector<BrokerConfirmation>& candidates) const;

    size_t auto_resolve_count() const { return auto_resolved_; }

private:
    MatchingConfig config_;
    mutable size_t auto_resolved_ = 0;
};

} // namespace recon
